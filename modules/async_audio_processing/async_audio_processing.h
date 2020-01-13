/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_ASYNC_AUDIO_PROCESSING_ASYNC_AUDIO_PROCESSING_H_
#define MODULES_ASYNC_AUDIO_PROCESSING_ASYNC_AUDIO_PROCESSING_H_

#include <functional>
#include <memory>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class AudioFrame;
class AudioFrameProcessor;
class TaskQueueFactory;

class AsyncAudioProcessing {
 public:
  virtual ~AsyncAudioProcessing() = default;
  AsyncAudioProcessing() = default;
  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void Process(std::unique_ptr<AudioFrame> frame) = 0;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(AsyncAudioProcessing);
};

class AsyncAudioProcessingFactory : public rtc::RefCountInterface {
 public:
  using OnFrameProcessedCallback =
      std::function<void(std::unique_ptr<AudioFrame>)>;

  ~AsyncAudioProcessingFactory();
  AsyncAudioProcessingFactory(AudioFrameProcessor* frame_processor,
                              TaskQueueFactory* task_queue_factory);

  std::unique_ptr<AsyncAudioProcessing> CreateAsyncAudioProcessing(
      OnFrameProcessedCallback on_frame_processed_callback);

 private:
  AudioFrameProcessor* const frame_processor_;
  TaskQueueFactory* const task_queue_factory_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AsyncAudioProcessingFactory);
};

}  // namespace webrtc

#endif  // MODULES_ASYNC_AUDIO_PROCESSING_ASYNC_AUDIO_PROCESSING_H_
