/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/aaudio_recorder.h"

#include "api/array_view.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

AAudioRecorder::AAudioRecorder(AudioManager* audio_manager)
    : aaudio_(audio_manager, AAUDIO_DIRECTION_INPUT, this) {
  RTC_LOG(INFO) << "ctor";
  thread_checker_aaudio_.DetachFromThread();
}

AAudioRecorder::~AAudioRecorder() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  // TODO(henrika): rename...
  RTC_LOG(INFO) << "detected underruns: " << underrun_count_;
}

int AAudioRecorder::Init() {
  RTC_LOG(INFO) << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK_EQ(aaudio_.audio_parameters().channels(), 1u);
  return 0;
}

int AAudioRecorder::Terminate() {
  RTC_LOG(INFO) << "Terminate";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  StopRecording();
  return 0;
}

int AAudioRecorder::InitRecording() {
  RTC_LOG(INFO) << "InitRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK(!recording_);
  if (!aaudio_.Init()) {
    return -1;
  }
  initialized_ = true;
  return 0;
}

int AAudioRecorder::StartRecording() {
  RTC_LOG(INFO) << "StartRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!recording_);
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }
  if (!aaudio_.Start()) {
    return -1;
  }
  // Track underrun count for statistics and automatic buffer adjustments.
  underrun_count_ = aaudio_.xrun_count();
  recording_ = true;
  return 0;
}

int AAudioRecorder::StopRecording() {
  RTC_LOG(INFO) << "StopRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !recording_) {
    return 0;
  }
  if (!aaudio_.Stop()) {
    return -1;
  }
  thread_checker_aaudio_.DetachFromThread();
  initialized_ = false;
  recording_ = false;
  return 0;
}

void AAudioRecorder::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  RTC_LOG(INFO) << "AttachAudioBuffer";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const AudioParameters audio_parameters = aaudio_.audio_parameters();
  audio_device_buffer_->SetRecordingSampleRate(audio_parameters.sample_rate());
  audio_device_buffer_->SetRecordingChannels(audio_parameters.channels());
  RTC_CHECK(audio_device_buffer_);
  // Create a modified audio buffer class which allows us to deliver any number
  // of samples (and not only multiples of 10ms which WebRTC uses) to match the
  // native AAudio buffer size.
  const size_t capacity = 5 * audio_parameters.GetBytesPer10msBuffer();
  fine_audio_buffer_.reset(new FineAudioBuffer(
      audio_device_buffer_, audio_parameters.sample_rate(), capacity));
}

int AAudioRecorder::EnableBuiltInAEC(bool enable) {
  RTC_LOG(INFO) << "EnableBuiltInAEC: " << enable;
  RTC_LOG(LS_ERROR) << "Not implemented";
  return 0;
}

int AAudioRecorder::EnableBuiltInAGC(bool enable) {
  RTC_LOG(INFO) << "EnableBuiltInAGC: " << enable;
  RTC_LOG(LS_ERROR) << "Not implemented";
  return 0;
}

int AAudioRecorder::EnableBuiltInNS(bool enable) {
  RTC_LOG(INFO) << "EnableBuiltInNS: " << enable;
  RTC_LOG(LS_ERROR) << "Not implemented";
  return 0;
}

// TODO(henrika): improve (possibly restart)...
void AAudioRecorder::OnErrorCallback(aaudio_result_t error) {
  RTC_LOG(LS_ERROR) << "OnErrorCallback: " << AAudio_convertResultToText(error);
}

// Render and write |num_frames| into |audio_data|.
aaudio_data_callback_result_t AAudioRecorder::OnDataCallback(
    void* audio_data,
    int32_t num_frames) {
  RTC_DCHECK(thread_checker_aaudio_.CalledOnValidThread());

  // TODO(henrika): fix latency estimation
  latency_millis_ = 25.0;

  // Copy recorded audio in |audio_data| to the WebRTC sink using the
  // FineAudioBuffer object.
  const size_t num_bytes =
      sizeof(int16_t) * aaudio_.samples_per_frame() * num_frames;
  fine_audio_buffer_->DeliverRecordedData(
      rtc::ArrayView<const int8_t>(static_cast<const int8_t*>(audio_data),
                                   num_bytes),
      static_cast<int>(latency_millis_ + 0.5));

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

}  // namespace webrtc
