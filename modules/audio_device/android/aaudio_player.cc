/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/aaudio_player.h"

// #include <trace.h>

#include "api/array_view.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

AAudioPlayer::AAudioPlayer(AudioManager* audio_manager)
    : aaudio_(audio_manager, AAUDIO_DIRECTION_OUTPUT, this) {
  RTC_LOG(INFO) << "ctor";
  thread_checker_aaudio_.DetachFromThread();
}

AAudioPlayer::~AAudioPlayer() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  RTC_LOG(INFO) << "detected underruns: " << underrun_count_;
}

int AAudioPlayer::Init() {
  RTC_LOG(INFO) << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK_EQ(aaudio_.audio_parameters().channels(), 1u);
  return 0;
}

int AAudioPlayer::Terminate() {
  RTC_LOG(INFO) << "Terminate";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  StopPlayout();
  return 0;
}

int AAudioPlayer::InitPlayout() {
  RTC_LOG(INFO) << "InitPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK(!playing_);
  if (!aaudio_.Init()) {
    return -1;
  }
  initialized_ = true;
  return 0;
}

int AAudioPlayer::StartPlayout() {
  RTC_LOG(INFO) << "StartPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!playing_);
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }
  if (!aaudio_.Start()) {
    return -1;
  }
  // Track underrun count for statistics and automatic buffer adjustments.
  underrun_count_ = aaudio_.xrun_count();
  playing_ = true;
  return 0;
}

int AAudioPlayer::StopPlayout() {
  RTC_LOG(INFO) << "StopPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !playing_) {
    return 0;
  }
  if (!aaudio_.Stop()) {
    return -1;
  }
  thread_checker_aaudio_.DetachFromThread();
  initialized_ = false;
  playing_ = false;
  return 0;
}

void AAudioPlayer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  RTC_LOG(INFO) << "AttachAudioBuffer";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const AudioParameters audio_parameters = aaudio_.audio_parameters();
  audio_device_buffer_->SetPlayoutSampleRate(audio_parameters.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(audio_parameters.channels());
  RTC_CHECK(audio_device_buffer_);
  // Create a modified audio buffer class which allows us to ask for any number
  // of samples (and not only multiple of 10ms) to match the optimal buffer
  // size per callback used by AAudio. Use an initial capacity of 50ms to ensure
  // that the buffer can cache old data and at the same time be prepared for
  // increased burst size in AAudio if buffer underruns are detected.
  const size_t capacity = 5 * audio_parameters.GetBytesPer10msBuffer();
  fine_audio_buffer_.reset(new FineAudioBuffer(
      audio_device_buffer_, audio_parameters.sample_rate(), capacity));
}

// TODO(henrika): improve (possibly restart)...
void AAudioPlayer::OnErrorCallback(aaudio_result_t error) {
  RTC_LOG(LS_ERROR) << "OnErrorCallback: " << AAudio_convertResultToText(error);
}

// Render and write |num_frames| into |audio_data|.
aaudio_data_callback_result_t AAudioPlayer::OnDataCallback(void* audio_data,
                                                           int32_t num_frames) {
  RTC_DCHECK(thread_checker_aaudio_.CalledOnValidThread());
  // Check if the underrun count has increased. If it has, increase the buffer
  // size by adding the size of a burst. It will reduce the risk of underruns
  // at the expense of an increased latency.
  // TODO(henrika): enable possibility to disable and/or tune the algorithm.
  const int32_t underrun_count = aaudio_.xrun_count();
  if (underrun_count > underrun_count_) {
    RTC_LOG(LS_ERROR) << "Underrun detected: " << underrun_count;
    underrun_count_ = underrun_count;
    aaudio_.IncreaseOutputBufferSize();
  }
  // Estimate latency between writing an audio frame to the output stream and
  // the time that same frame is played out on the output audio device.
  latency_millis_ = aaudio_.EstimateLatencyMillis();
  // Read audio data from the WebRTC source using the FineAudioBuffer object
  // and write that data into |audio_data| to be played out by AAudio.
  const size_t num_bytes =
      sizeof(int16_t) * aaudio_.samples_per_frame() * num_frames;
  fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int8_t>(static_cast<int8_t*>(audio_data), num_bytes),
      static_cast<int>(latency_millis_ + 0.5));

  // TODO(henrika): possibly add trace here to be included in systrace.
  // See https://developer.android.com/studio/profile/systrace-commandline.html.
  // const int32_t buffer_size = aaudio_.buffer_size_in_frames();
  // Trace::beginSection("num_frames %d, underruns %d, buffer size %d",
  //                    num_frames, underrun_count, buffer_size);

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

}  // namespace webrtc
