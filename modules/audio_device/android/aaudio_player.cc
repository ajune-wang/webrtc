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
  // Detach from this thread since we want to use the checker to verify calls
  // from the real-time thread owned by AAudio.
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
  if (aaudio_.audio_parameters().channels() == 2) {
    // TODO(henrika): FineAudioBuffer needs more work to support stereo.
    RTC_LOG(LS_ERROR) << "AAudioPlayer does not support stereo";
    return -1;
  }
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

  aaudio_.Init();

  // TODO(henrika): fix return values.
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
  last_play_time_ = rtc::Time();
  if (!aaudio_.Start()) {
    return -1;
  }
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
  // TODO(henrika): fix comments about capacity.
  // Create a modified audio buffer class which allows us to ask for any number
  // of samples (and not only multiple of 10ms) to match the optimal buffer
  // size per callback used by AAudio.
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
  // RTC_DCHECK_EQ(stream, stream_);
  RTC_DCHECK(thread_checker_aaudio_.CalledOnValidThread());
  // RTC_LOG(INFO) << "OnDataCallback: " << num_frames;

  // const uint32_t current_time = rtc::TimeMillis();
  // const uint32_t diff = current_time - last_play_time_;
  // last_play_time_ = current_time;
  // RTC_LOG(INFO) << "dt: " << diff;

  // TODO(henrika): implement buffer adjustment here by increasing buffer size
  // with one burst.
  const int32_t underrun_count = aaudio_.xrun_count();
  if (underrun_count > underrun_count_) {
    RTC_LOG(LS_ERROR) << "Underrun detected: " << underrun_count;
    underrun_count_ = underrun_count;
  }

  // Read audio data from the WebRTC source using the FineAudioBuffer object
  // and write that data into |audio_data| to be played out by AAudio.

  const size_t num_bytes =
      sizeof(int16_t) * aaudio_.samples_per_frame() * num_frames;
  fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int8_t>(static_cast<int8_t*>(audio_data), num_bytes));

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

/*
// Get the time at which a particular frame was presented.
// Results are only valid when the stream is in AAUDIO_STREAM_STATE_STARTED.
int64_t frame_index;
int64_t time_nano_secs;
aaudio_result_t result = AAudioStream_getTimestamp(
    stream_, CLOCK_MONOTONIC, &frame_index, &time_nano_secs);
if (result == AAUDIO_OK) {
  // The time at which a particular frame was presented.
  RTC_LOG(INFO) << "frame_index: " << frame_index;
  // Number of written frames since frame was created.
  RTC_LOG(INFO) << "frames written: "
               << AAudioStream_getFramesWritten(stream_);
}*/

}  // namespace webrtc
