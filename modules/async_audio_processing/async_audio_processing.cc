
/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/async_audio_processing/async_audio_processing.h"

#include <utility>

#include "api/audio/audio_frame.h"
#include "api/audio/audio_frame_processor.h"
#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

namespace {

class AsyncAudioProcessingImpl : public AsyncAudioProcessing,
                                 public AudioFrameProcessor::Sink {
 public:
  using OnFrameProcessedCallback =
      std::function<void(std::unique_ptr<AudioFrame>)>;

  ~AsyncAudioProcessingImpl() override;
  AsyncAudioProcessingImpl(
      AudioFrameProcessor* frame_processor,
      TaskQueueFactory* task_queue_factory,
      OnFrameProcessedCallback on_frame_processed_callback);

  void Start() override;
  void Stop() override;
  void Process(std::unique_ptr<AudioFrame> frame) override;

  void OnFrameProcessed(std::unique_ptr<AudioFrame> frame) override;

 private:
  void SetProcessorSink(AudioFrameProcessor::Sink* sink);

  OnFrameProcessedCallback on_frame_processed_callback_;
  AudioFrameProcessor* const frame_processor_;
  rtc::TaskQueue task_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AsyncAudioProcessingImpl);
};

AsyncAudioProcessingImpl::~AsyncAudioProcessingImpl() {
  Stop();  // ???
}
AsyncAudioProcessingImpl::AsyncAudioProcessingImpl(
    AudioFrameProcessor* frame_processor,
    TaskQueueFactory* task_queue_factory,
    OnFrameProcessedCallback on_frame_processed_callback)
    : on_frame_processed_callback_(std::move(on_frame_processed_callback)),
      frame_processor_(frame_processor),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "AsyncAudioProcessing",
          TaskQueueFactory::Priority::NORMAL)) {}

void AsyncAudioProcessingImpl::Start() {
  task_queue_.PostTask([this]() { this->SetProcessorSink(this); });
}

void AsyncAudioProcessingImpl::Stop() {
  task_queue_.PostTask([this]() { this->SetProcessorSink(nullptr); });
}

void AsyncAudioProcessingImpl::Process(std::unique_ptr<AudioFrame> frame) {
  task_queue_.PostTask([this, frame = std::move(frame)]() mutable {
    this->frame_processor_->Process(std::move(frame));
  });
}

void AsyncAudioProcessingImpl::OnFrameProcessed(
    std::unique_ptr<AudioFrame> frame) {
  /*
  if (task_queue_->IsCurrent()) {
    on_frame_processed_callback_(std::move(frame));
    return;
  }*/
  task_queue_.PostTask([this, frame = std::move(frame)]() mutable {
    this->on_frame_processed_callback_(std::move(frame));
  });
}

void AsyncAudioProcessingImpl::SetProcessorSink(
    AudioFrameProcessor::Sink* sink) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  frame_processor_->SetSink(sink);
}

}  // namespace

AsyncAudioProcessingFactory::~AsyncAudioProcessingFactory() = default;
AsyncAudioProcessingFactory::AsyncAudioProcessingFactory(
    AudioFrameProcessor* frame_processor,
    TaskQueueFactory* task_queue_factory)
    : frame_processor_(frame_processor),
      task_queue_factory_(task_queue_factory) {}

std::unique_ptr<AsyncAudioProcessing>
AsyncAudioProcessingFactory::CreateAsyncAudioProcessing(
    OnFrameProcessedCallback on_frame_processed_callback) {
  return std::make_unique<AsyncAudioProcessingImpl>(
      frame_processor_, task_queue_factory_,
      std::move(on_frame_processed_callback));
}

}  // namespace webrtc
