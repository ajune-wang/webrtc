/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_FRAME_ANALYZER_VIDEO_GEOMETRY_ALIGNER_H_
#define RTC_TOOLS_FRAME_ANALYZER_VIDEO_GEOMETRY_ALIGNER_H_

#include <array>

#include "rtc_tools/video_file_reader.h"

namespace webrtc {
namespace test {

// Represents a linear geometry transformation from [x, y] to [x, y'] through
// the equation: [x', y'] = [x, y, 1] * matrix.
using GeometryTransformationMatrix = std::array<std::array<float, 3>, 2>;

// Calculate the optimal geometry transformation that should be applied to the
// test video to match as closely as possible to the reference video.
GeometryTransformationMatrix CalculateGeometryTransformationMatrix(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video);

// Calculate geometry transformation for a single I420 frame.
GeometryTransformationMatrix CalculateGeometryTransformationMatrix(
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame,
    const rtc::scoped_refptr<I420BufferInterface>& test_frame);

// Apply a geometry transformation to a video.
rtc::scoped_refptr<Video> AdjustGeometry(
    const GeometryTransformationMatrix& geometry_matrix,
    const rtc::scoped_refptr<Video>& video);

// Apply a geometry transformation to a single I420 frame.
rtc::scoped_refptr<I420BufferInterface> AdjustGeometry(
    const GeometryTransformationMatrix& geometry_matrix,
    const rtc::scoped_refptr<I420BufferInterface>& frame);

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_FRAME_ANALYZER_VIDEO_GEOMETRY_ALIGNER_H_
