/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"

#include <vector>

#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_tools/frame_analyzer/linear_least_squares.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

#include <iostream>

namespace webrtc {
namespace test {

namespace {

std::vector<int16_t> DerivativeX(const std::vector<uint8_t>& data,
                                 int width,
                                 int height) {
  std::vector<int16_t> res(width * height);
  for (int y = 0; y < height; ++y) {
    res[y * width + 0] = 2 * (static_cast<int16_t>(data[y * width + 1]) -
                              static_cast<int16_t>(data[y * width + 0]));
    res[y * width + width - 1] =
        2 * (static_cast<int16_t>(data[y * width + width - 1]) -
             static_cast<int16_t>(data[y * width + width - 2]));
    for (int x = 1; x < width - 1; ++x) {
      res[y * width + x] = static_cast<int16_t>(data[y * width + x + 1]) -
                           static_cast<int16_t>(data[y * width + x - 1]);
    }
  }
  return res;
}

// TODO(magjed): I could use transpose here.
std::vector<int16_t> DerivativeY(const std::vector<uint8_t>& data,
                                 int width,
                                 int height) {
  std::vector<int16_t> res(width * height);
  for (int x = 0; x < width; ++x) {
    res[x] = 2 * (static_cast<int16_t>(data[1 * width + x]) -
                  static_cast<int16_t>(data[0 * width + x]));
    res[(height - 1) * width + x] =
        2 * (static_cast<int16_t>(data[(height - 1) * width + x]) -
             static_cast<int16_t>(data[(height - 2) * width + x]));
    for (int y = 1; y < height - 1; ++y) {
      res[y * width + x] = static_cast<int16_t>(data[(y + 1) * width + x]) -
                           static_cast<int16_t>(data[(y - 1) * width + x]);
    }
  }
  return res;
}

GeometryTransformationMatrix VectorToGeometryMatrix(
    const std::vector<std::vector<double>>& v) {
  RTC_CHECK_EQ(1u, v.size());
  GeometryTransformationMatrix geometry_transformation;

  geometry_transformation[0][0] = 1.0f + v[0][0];
  geometry_transformation[0][1] = v[0][2];
  geometry_transformation[0][2] = v[0][4];

  geometry_transformation[1][0] = v[0][1];
  geometry_transformation[1][1] = 1.0f + v[0][3];
  geometry_transformation[1][2] = v[0][5];

  // const GeometryTransformationMatrix kIdentityGeometryMatrix = {
  //     {{1, 0, 0}, {0, 1, 0}}};

  return geometry_transformation;
}

void AddGeometryObservations(
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame,
    const rtc::scoped_refptr<I420BufferInterface>& test_frame,
    IncrementalLinearLeastSquares* lls) {
  std::vector<uint8_t> ref_y_plane(reference_frame->width() *
                                   reference_frame->height());
  libyuv::CopyPlane(reference_frame->DataY(), reference_frame->StrideY(),
                    ref_y_plane.data(), reference_frame->width(),
                    reference_frame->width(), reference_frame->height());

  std::vector<int16_t> derivative_x = DerivativeX(
      ref_y_plane, reference_frame->width(), reference_frame->height());
  std::vector<int16_t> derivative_y = DerivativeY(
      ref_y_plane, reference_frame->width(), reference_frame->height());

  // TODO(magjed): It's actually 9 bits * 11 bits = 20 bits in the worst case.
  std::vector<std::vector<int16_t>> left_hand(6);
  std::vector<std::vector<int16_t>> right_hand(1);
  for (int y = 0; y < reference_frame->height(); ++y) {
    for (int x = 0; x < reference_frame->width(); ++x) {
      const int16_t diff =
          static_cast<int16_t>(
              reference_frame->DataY()[reference_frame->StrideY() * y + x]) -
          static_cast<int16_t>(
              test_frame->DataY()[test_frame->StrideY() * y + x]);
      right_hand[0].push_back(2 * diff);

      const int16_t dx = derivative_x[reference_frame->width() * y + x];
      const int16_t dy = derivative_y[reference_frame->width() * y + x];
      left_hand[0].push_back(x * dx);
      left_hand[1].push_back(x * dy);

      left_hand[2].push_back(y * dx);
      left_hand[3].push_back(y * dy);

      left_hand[4].push_back(dx);
      left_hand[5].push_back(dy);
    }
  }

  lls->AddObservations2(left_hand, right_hand);
}

// Adjust geometry for a single plane.
void AdjustGeometry(const GeometryTransformationMatrix& matrix,
                    const uint8_t* src,
                    int src_stride,
                    uint8_t* dst,
                    int dst_stride,
                    int width,
                    int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float new_x_float = matrix[0][0] * x + matrix[0][1] * y + matrix[0][2];
      float new_y_float = matrix[1][0] * x + matrix[1][1] * y + matrix[1][2];
      new_x_float = std::max(0.f, std::min(new_x_float, width - 1.f));
      new_y_float = std::max(0.f, std::min(new_y_float, height - 1.f));
      const int new_x_int = std::min(width - 2, static_cast<int>(new_x_float));
      const int new_y_int = std::min(height - 2, static_cast<int>(new_y_float));

      const int base_index = src_stride * new_y_int + new_x_int;
      const float val0 = src[base_index];
      const float val1 = src[base_index + 1];
      const float val2 = src[base_index + src_stride];
      const float val3 = src[base_index + src_stride + 1];

      const float x_fraction = new_x_float - new_x_int;
      const float y_fraction = new_y_float - new_y_int;

      const float average0 = val0 + (val1 - val0) * x_fraction;
      const float average1 = val2 + (val3 - val2) * x_fraction;

      const float average = average0 + (average1 - average0) * y_fraction;

      dst[y * dst_stride + x] = static_cast<uint8_t>(std::round(average));
    }
  }
}

GeometryTransformationMatrix CalculateGeometryTransformationMatrixSingle(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  IncrementalLinearLeastSquares lls;
  RTC_CHECK_EQ(reference_video->number_of_frames(),
               test_video->number_of_frames());
  for (size_t i = 0; i < reference_video->number_of_frames(); ++i) {
    AddGeometryObservations(reference_video->GetFrame(i),
                            test_video->GetFrame(i), &lls);
  }
  return VectorToGeometryMatrix(lls.GetBestSolution());
}

GeometryTransformationMatrix Multiply(const GeometryTransformationMatrix& a,
                                      const GeometryTransformationMatrix& b) {
  GeometryTransformationMatrix c;
  c[0][0] = a[0][0] * b[0][0] + a[1][0] * b[0][1];  // + b[0][2];
  c[0][1] = a[0][1] * b[0][0] + a[1][1] * b[0][1];  // + b[0][2];
  c[0][2] = a[0][2] * b[0][0] + a[1][2] * b[0][1];  // + b[0][2];

  c[1][0] = a[0][0] * b[1][0] + a[1][0] * b[1][1];  // + b[1][2];
  c[1][1] = a[0][1] * b[1][0] + a[1][1] * b[1][1];  // + b[1][2];
  c[1][2] = a[0][2] * b[1][0] + a[1][2] * b[1][1];  // + b[1][2];

  return c;
}

}  // namespace

GeometryTransformationMatrix CalculateGeometryTransformationMatrix(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  GeometryTransformationMatrix matrix =
      CalculateGeometryTransformationMatrixSingle(reference_video, test_video);

  for (int i = 0; i < 10; ++i) {
    GeometryTransformationMatrix incremental_matrix =
        CalculateGeometryTransformationMatrixSingle(
            AdjustGeometry(matrix, reference_video), test_video);

    char buf2[256];
    rtc::SimpleStringBuilder string_builder2(buf2);
    for (int i = 0; i < 2; ++i) {
      string_builder2 << "\n";
      for (int j = 0; j < 3; ++j)
        string_builder2.AppendFormat("%6.3f ", matrix[i][j]);
    }
    std::cerr << "Matrix: " << string_builder2.str() << std::endl;

    char buf[256];
    rtc::SimpleStringBuilder string_builder(buf);
    for (int i = 0; i < 2; ++i) {
      string_builder << "\n";
      for (int j = 0; j < 3; ++j)
        string_builder.AppendFormat("%6.3f ", incremental_matrix[i][j]);
    }
    std::cerr << "Incremental matrix: " << string_builder.str() << std::endl;

    matrix = Multiply(matrix, incremental_matrix);
  }

  return matrix;
}

GeometryTransformationMatrix CalculateGeometryTransformationMatrix(
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame,
    const rtc::scoped_refptr<I420BufferInterface>& test_frame) {
  IncrementalLinearLeastSquares lls;
  AddGeometryObservations(reference_frame, test_frame, &lls);
  return VectorToGeometryMatrix(lls.GetBestSolution());
}

// Apply a geometry transformation to a video.
rtc::scoped_refptr<Video> AdjustGeometry(
    const GeometryTransformationMatrix& geometry_transformation,
    const rtc::scoped_refptr<Video>& video) {
  class GeometryAdjustedVideo : public rtc::RefCountedObject<Video> {
   public:
    GeometryAdjustedVideo(
        const GeometryTransformationMatrix& geometry_transformation,
        const rtc::scoped_refptr<Video>& video)
        : geometry_transformation_(geometry_transformation), video_(video) {}

    int width() const override { return video_->width(); }
    int height() const override { return video_->height(); }
    size_t number_of_frames() const override {
      return video_->number_of_frames();
    }

    rtc::scoped_refptr<I420BufferInterface> GetFrame(
        size_t index) const override {
      return AdjustGeometry(geometry_transformation_, video_->GetFrame(index));
    }

   private:
    const GeometryTransformationMatrix geometry_transformation_;
    const rtc::scoped_refptr<Video> video_;
  };

  return new GeometryAdjustedVideo(geometry_transformation, video);
}

rtc::scoped_refptr<I420BufferInterface> AdjustGeometry(
    const GeometryTransformationMatrix& matrix,
    const rtc::scoped_refptr<I420BufferInterface>& frame) {
  const int width = frame->width();
  const int height = frame->height();

  rtc::scoped_refptr<I420Buffer> new_frame = I420Buffer::Create(width, height);

  GeometryTransformationMatrix chroma_matrix = matrix;
  chroma_matrix[0][2] /= 2;
  chroma_matrix[1][2] /= 2;

  AdjustGeometry(matrix, frame->DataY(), frame->StrideY(),
                 new_frame->MutableDataY(), new_frame->StrideY(),
                 frame->width(), frame->height());
  AdjustGeometry(chroma_matrix, frame->DataU(), frame->StrideU(),
                 new_frame->MutableDataU(), new_frame->StrideU(),
                 frame->ChromaWidth(), frame->ChromaHeight());
  AdjustGeometry(chroma_matrix, frame->DataV(), frame->StrideV(),
                 new_frame->MutableDataV(), new_frame->StrideV(),
                 frame->ChromaWidth(), frame->ChromaHeight());

  return new_frame;
}

}  // namespace test
}  // namespace webrtc
