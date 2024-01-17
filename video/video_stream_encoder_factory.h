/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_VIDEO_STREAM_ENCODER_FACTORY_H_
#define VIDEO_VIDEO_STREAM_ENCODER_FACTORY_H_

#include <memory>

#include "api/field_trials_view.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "system_wrappers/include/clock.h"
#include "video/send_statistics_proxy.h"
#include "video/video_stream_encoder.h"

namespace webrtc {

class VideoStreamEncoderFactory {
 public:
  VideoStreamEncoderFactory() = default;
  virtual ~VideoStreamEncoderFactory() = default;

  virtual std::unique_ptr<VideoStreamEncoder> Create(
      Clock& clock,
      int num_cpu_cores,
      TaskQueueFactory& task_queue_factory,
      SendStatisticsProxy& stats_proxy,
      const VideoStreamEncoderSettings& encoder_settings,
      VideoStreamEncoder::BitrateAllocationCallbackType
          bitrate_allocation_callback_type,
      const FieldTrialsView& field_trials,
      Metronome* metronome,
      webrtc::VideoEncoderFactory::EncoderSelectorInterface* encoder_selector);
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_ENCODER_FACTORY_H_
