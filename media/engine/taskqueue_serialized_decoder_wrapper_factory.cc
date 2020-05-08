/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "media/engine/taskqueue_serialized_decoder_wrapper_factory.h"

#include <deque>
#include <utility>

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/event.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

struct SyncEvent {
  int32_t Wait() {
    event.Wait(rtc::Event::kForever);
    return val;
  }

  void Set(int32_t return_val) {
    val = return_val;
    event.Set();
  }

  bool IsSet() { return event.Wait(0); }
  int32_t Val() const { return val; }

 private:
  rtc::Event event;
  int32_t val = 0;
};

class TaskQueueSerializedDecoderWrapper : public VideoDecoder {
 public:
  TaskQueueSerializedDecoderWrapper(std::unique_ptr<VideoDecoder> decoder,
                                    rtc::TaskQueue* const task_queue)
      : task_queue_(task_queue),
        decoder_(std::move(decoder)),
        implementation_name_(decoder_->ImplementationName()),
        prefers_late_decoding_(decoder_->PrefersLateDecoding()) {}

  // Post a synchronous tasks for InitDecode(), Release() and
  // RegisterDecodeCompleteCallback().
  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    SyncEvent sync_event;
    task_queue_->PostTask([&]() {
      RTC_DCHECK_RUN_ON(task_queue_);
      sync_event.Set(decoder_->InitDecode(codec_settings, number_of_cores));
    });
    return sync_event.Wait();
  }

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override {
    SyncEvent sync_event;
    task_queue_->PostTask([&]() {
      RTC_DCHECK_RUN_ON(task_queue_);
      sync_event.Set(decoder_->RegisterDecodeCompleteCallback(callback));
    });
    return sync_event.Wait();
  }

  int32_t Release() override {
    SyncEvent sync_event;
    task_queue_->PostTask([&]() {
      RTC_DCHECK_RUN_ON(task_queue_);
      sync_event.Set(decoder_->Release());
    });
    int32_t ret = sync_event.Wait();
    DrainAllPending();
    return ret;
  }

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    int32_t prev_frame_status = DrainCompletedAndGetErrorCode();
    if (prev_frame_status != WEBRTC_VIDEO_CODEC_OK) {
      return prev_frame_status;
    }

    if (pending_.size() >= kMaxPendingFrames) {
      prev_frame_status = pending_.front().Wait();
      pending_.pop_front();
      if (prev_frame_status != WEBRTC_VIDEO_CODEC_OK) {
        return prev_frame_status;
      }
    }

    pending_.emplace_back();
    task_queue_->PostTask([&, image = input_image, &sync = pending_.back()]() {
      RTC_DCHECK_RUN_ON(task_queue_);
      sync.Set(decoder_->Decode(image, missing_frames, render_time_ms));
    });
    return WEBRTC_VIDEO_CODEC_OK;
  }

  bool PrefersLateDecoding() const override { return prefers_late_decoding_; }

  const char* ImplementationName() const override {
    return implementation_name_;
  }

 private:
  int32_t DrainCompletedAndGetErrorCode() {
    int32_t ret = WEBRTC_VIDEO_CODEC_OK;
    while (!pending_.empty() && pending_.front().IsSet()) {
      // Grab the only the first non-ok error code, if any.
      if (ret == WEBRTC_VIDEO_CODEC_OK) {
        ret = pending_.front().Val();
      }
      pending_.pop_front();
    }
    return ret;
  }

  void DrainAllPending() {
    while (!pending_.empty()) {
      pending_.front().Wait();
      pending_.pop_front();
    }
  }

  static constexpr size_t kMaxPendingFrames = 6;
  rtc::TaskQueue* const task_queue_;
  const std::unique_ptr<VideoDecoder> decoder_ RTC_GUARDED_BY(task_queue_);
  const char* implementation_name_;
  const bool prefers_late_decoding_;
  std::deque<SyncEvent> pending_;
};

TaskQueueSerializedDecoderWrapperFactory::
    TaskQueueSerializedDecoderWrapperFactory(
        TaskQueueFactory* taskqueue_factory,
        std::unique_ptr<VideoDecoderFactory> decoder_factory)
    : decoder_factory_(std::move(decoder_factory)),
      task_queue_(taskqueue_factory->CreateTaskQueue(
          "DecoderQueue",
          TaskQueueFactory::Priority::HIGH)) {}

std::vector<SdpVideoFormat>
TaskQueueSerializedDecoderWrapperFactory::GetSupportedFormats() const {
  return decoder_factory_->GetSupportedFormats();
}

std::unique_ptr<VideoDecoder>
TaskQueueSerializedDecoderWrapperFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  return std::make_unique<TaskQueueSerializedDecoderWrapper>(
      decoder_factory_->CreateVideoDecoder(format), &task_queue_);
}

}  // namespace webrtc
