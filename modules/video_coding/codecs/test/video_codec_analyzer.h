/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_ANALYZER_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_ANALYZER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/test/videocodec_test_stats_impl.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue_for_test.h"

namespace webrtc {
namespace test {

class VideoCodecAnalyzer {
 public:
  class ReferenceVideoSource {
   public:
    virtual ~ReferenceVideoSource() = default;
    virtual VideoFrame GetFrame(uint32_t timestamp_rtp) = 0;
  };

  explicit VideoCodecAnalyzer(ReferenceVideoSource* reference_video_source);

  void StartEncode(const VideoFrame& frame);

  void FinishEncode(const EncodedImage& frame);

  void StartDecode(const EncodedImage& frame);

  void FinishDecode(const VideoFrame& frame, int spatial_idx);

  std::unique_ptr<VideoCodecTestStats> GetStats();

 protected:
  ReferenceVideoSource* const reference_video_source_;
  TaskQueueForTest task_queue_;
  VideoCodecTestStatsImpl stats_ RTC_GUARDED_BY(sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_ANALYZER_H_
