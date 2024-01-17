/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/video_stream_encoder_factory.h"

#include <memory>
#include <utility>

#include "api/field_trials_view.h"
#include "api/metronome/metronome.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "system_wrappers/include/clock.h"
#include "video/adaptation/overuse_frame_detector.h"
#include "video/frame_cadence_adapter.h"
#include "video/send_statistics_proxy.h"
#include "video/video_stream_encoder.h"

namespace webrtc {

std::unique_ptr<VideoStreamEncoder> VideoStreamEncoderFactory::Create(
    Clock& clock,
    int num_cpu_cores,
    TaskQueueFactory& task_queue_factory,
    SendStatisticsProxy& stats_proxy,
    const VideoStreamEncoderSettings& encoder_settings,
    VideoStreamEncoder::BitrateAllocationCallbackType
        bitrate_allocation_callback_type,
    const FieldTrialsView& field_trials,
    Metronome* metronome,
    webrtc::VideoEncoderFactory::EncoderSelectorInterface* encoder_selector) {
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> encoder_queue =
      task_queue_factory.CreateTaskQueue("EncoderQueue",
                                         TaskQueueFactory::Priority::NORMAL);
  TaskQueueBase* encoder_queue_ptr = encoder_queue.get();
  return std::make_unique<VideoStreamEncoder>(
      &clock, num_cpu_cores, &stats_proxy, encoder_settings,
      std::make_unique<OveruseFrameDetector>(&stats_proxy),
      FrameCadenceAdapterInterface::Create(
          &clock, encoder_queue_ptr, metronome,
          /*worker_queue=*/TaskQueueBase::Current(), field_trials),
      std::move(encoder_queue), bitrate_allocation_callback_type, field_trials,
      encoder_selector);
}

}  // namespace webrtc
