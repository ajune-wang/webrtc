/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_color_aligner.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <vector>

#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_tools/frame_analyzer/linear_least_squares.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {

// Helper function for AdjustColors(). This functions calculates a single output
// row for either y, u, or v, with the given color coefficients.
void CalculateSingleColorChannel(const uint8_t* y_data,
                                 const uint8_t* u_data,
                                 const uint8_t* v_data,
                                 const std::array<float, 4>& coeff,
                                 size_t n,
                                 uint8_t* output) {
  for (size_t i = 0; i < n; ++i) {
    const float val = coeff[0] * y_data[i] + coeff[1] * u_data[i] +
                      coeff[2] * v_data[i] + coeff[3];
    // Clamp result to a byte.
    output[i] =
        static_cast<uint8_t>(std::round(std::max(0.0f, std::min(val, 255.0f))));
  }
}

// Convert a frame to four vectors consisting of [y, u, v, 1].
std::vector<std::vector<uint8_t>> FlattenYuvData(
    const rtc::scoped_refptr<I420BufferInterface>& frame) {
  std::vector<std::vector<uint8_t>> result(
      4, std::vector<uint8_t>(frame->width() * frame->height()));

  // Downscale the Y plane so that all YUV planes are the same size.
  libyuv::ScalePlane(frame->DataY(), frame->StrideY(), frame->width(),
                     frame->height(), result[0].data(), frame->ChromaWidth(),
                     frame->ChromaWidth(), frame->ChromaHeight(),
                     libyuv::kFilterBox);

  libyuv::CopyPlane(frame->DataU(), frame->StrideU(), result[1].data(),
                    frame->ChromaWidth(), frame->ChromaWidth(),
                    frame->ChromaHeight());

  libyuv::CopyPlane(frame->DataV(), frame->StrideV(), result[2].data(),
                    frame->ChromaWidth(), frame->ChromaWidth(),
                    frame->ChromaHeight());

  std::fill(result[3].begin(), result[3].end(), 1u);

  return result;
}

}  // namespace

ColorTransformationMatrix CalculateColorTransformationMatrix(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  RTC_CHECK_GE(reference_video->number_of_frames(),
               test_video->number_of_frames());

  IncrementalLinearLeastSquares incremental_lls;
  for (size_t i = 0; i < test_video->number_of_frames(); ++i) {
    incremental_lls.AddObservations(
        FlattenYuvData(test_video->GetFrame(i)),
        FlattenYuvData(reference_video->GetFrame(i)));
  }

  const std::vector<std::vector<double>> lls_solution =
      incremental_lls.GetBestSolution();

  ColorTransformationMatrix color_transformation;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      color_transformation[i][j] = lls_solution[i][j];
  }

  return color_transformation;
}

rtc::scoped_refptr<I420BufferInterface> AdjustColors(
    const ColorTransformationMatrix& color_matrix,
    const rtc::scoped_refptr<I420BufferInterface>& frame) {
  // Allocate I420 buffer that will hold the color adjusted frame.
  rtc::scoped_refptr<I420Buffer> adjusted_frame =
      I420Buffer::Create(frame->width(), frame->height());

  // Fill in the adjusted data row by row.
  for (int y = 0; y < frame->height(); ++y) {
    const int half_y = y / 2;
    const uint8_t* y_row = frame->DataY() + frame->StrideY() * y;
    const uint8_t* u_row = frame->DataU() + frame->StrideU() * half_y;
    const uint8_t* v_row = frame->DataV() + frame->StrideV() * half_y;

    CalculateSingleColorChannel(
        y_row, u_row, v_row, color_matrix[0], frame->width(),
        adjusted_frame->MutableDataY() + adjusted_frame->StrideY() * y);

    // Chroma channels only exist every second row for I420.
    if (y % 2 == 0) {
      CalculateSingleColorChannel(
          y_row, u_row, v_row, color_matrix[1], frame->ChromaWidth(),
          adjusted_frame->MutableDataU() + adjusted_frame->StrideU() * half_y);
      CalculateSingleColorChannel(
          y_row, u_row, v_row, color_matrix[2], frame->ChromaWidth(),
          adjusted_frame->MutableDataV() + adjusted_frame->StrideV() * half_y);
    }
  }

  return adjusted_frame;
}

rtc::scoped_refptr<Video> AdjustColors(
    const ColorTransformationMatrix& color_transformation,
    const rtc::scoped_refptr<Video>& video) {
  class ColorAdjustedVideo : public rtc::RefCountedObject<Video> {
   public:
    ColorAdjustedVideo(const ColorTransformationMatrix& color_transformation,
                       const rtc::scoped_refptr<Video>& video)
        : color_transformation_(color_transformation), video_(video) {}

    size_t number_of_frames() const override {
      return video_->number_of_frames();
    }

    rtc::scoped_refptr<I420BufferInterface> GetFrame(
        size_t index) const override {
      return AdjustColors(color_transformation_, video_->GetFrame(index));
    }

   private:
    const ColorTransformationMatrix color_transformation_;
    const rtc::scoped_refptr<Video> video_;
  };

  return new ColorAdjustedVideo(color_transformation, video);
}

}  // namespace test
}  // namespace webrtc
