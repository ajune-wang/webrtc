/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_ANALYSER_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_ANALYSER_H_

#include <memory>

#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/test/videocodec_test_stats_impl.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

class VideoCodecAnalyser {
 public:
  class VideoFrameProvider {
   public:
    virtual ~VideoFrameProvider() = default;

    virtual std::unique_ptr<VideoFrame> GetFrame(uint32_t timestamp_rtp) = 0;
  };

  struct CodingSettings {
    int bitrate_kbps;
    int framerate_fps;
  };

  explicit VideoCodecAnalyser(VideoFrameProvider* reference_frame_provider);

  void EncodeStarted(const VideoFrame& frame);

  void EncodeFinished(const EncodedImage& frame,
                      const CodingSettings& settings);

  void DecodeStarted(const EncodedImage& frame);

  void DecodeFinished(const VideoFrame& frame, int spatial_idx);

  void FinishAnalysis();

  std::unique_ptr<VideoCodecTestStats> GetStats();

 protected:
  VideoCodecTestStats::FrameStatistics* GetFrame(uint32_t timestamp_rtp,
                                                 int spatial_idx);

  VideoFrameProvider* const reference_frame_provider_;

  TaskQueueForTest quality_processing_task_queue_;

  VideoCodecTestStatsImpl stats_ RTC_GUARDED_BY(stats_mutex_);

  Mutex stats_mutex_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_ANALYSER_H_
