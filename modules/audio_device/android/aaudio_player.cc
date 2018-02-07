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
#include "rtc_base/atomicops.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

enum AudioDeviceMessageType : uint32_t {
  kMessageOutputStreamDisconnected,
};

// Returns the elapsed time since start based on monotonically increasing
// |num_frames| and the current |sample_rate|.
// static double TimeSinceStartMillis(int64_t num_frames, int32_t sample_rate) {
//   return static_cast<double>(num_frames) / sample_rate *
//          rtc::kNumMillisecsPerSec;
// }

AAudioPlayer::AAudioPlayer(AudioManager* audio_manager)
    : main_thread_(rtc::Thread::Current()),
      aaudio_(audio_manager, AAUDIO_DIRECTION_OUTPUT, this) {
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

void AAudioPlayer::OnErrorCallback(aaudio_result_t error) {
  RTC_LOG(LS_ERROR) << "OnErrorCallback: " << AAudio_convertResultToText(error);
  if (aaudio_.stream_state() == AAUDIO_STREAM_STATE_DISCONNECTED) {
    // The stream is disconnected and any attempt to use it will return
    // AAUDIO_ERROR_DISCONNECTED.
    RTC_LOG(WARNING) << "Output stream disconnected";
    // AAudio documentation states: "You should not close or reopen the stream
    // from the callback, use another thread instead". A message is therefore
    // sent to the main thread to do the restart operation.
    RTC_DCHECK(main_thread_);
    if (rtc::AtomicOps::CompareAndSwap(&restart_count_, 0, 1) == 0) {
      RTC_LOG(WARNING) << "___Restarting output: " << restart_count_;
      main_thread_->Post(RTC_FROM_HERE, this, kMessageOutputStreamDisconnected);
    }
  }
}

// Render and write |num_frames| into |audio_data|.
aaudio_data_callback_result_t AAudioPlayer::OnDataCallback(void* audio_data,
                                                           int32_t num_frames) {
  RTC_DCHECK(thread_checker_aaudio_.CalledOnValidThread());
  // RTC_LOG(INFO) << "OnDataCallback: " << num_frames;
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
  // TODO(henrika): use for development only.
  if (aaudio_.frames_written() % (1000 * aaudio_.frames_per_burst()) == 0) {
    RTC_DLOG(INFO) << "latency: " << latency_millis_;
  }
  // Read audio data from the WebRTC source using the FineAudioBuffer object
  // and write that data into |audio_data| to be played out by AAudio.
  const size_t num_bytes =
      sizeof(int16_t) * aaudio_.samples_per_frame() * num_frames;
  // Prime output with zeros during a short initial phase to avoid distorion.
  if (aaudio_.frames_written() < 100 * aaudio_.frames_per_burst()) {
    memset(static_cast<int8_t*>(audio_data), 0, num_bytes);
  } else {
    fine_audio_buffer_->GetPlayoutData(
        rtc::ArrayView<int8_t>(static_cast<int8_t*>(audio_data), num_bytes),
        static_cast<int>(latency_millis_ + 0.5));
  }

  // TODO(henrika): possibly add trace here to be included in systrace.
  // See https://developer.android.com/studio/profile/systrace-commandline.html.
  // Note that we must also initialize the trace functions, this enables you to
  // output trace statements without blocking.
  // Trace::initialize();
  // const int32_t buffer_size = aaudio_.buffer_size_in_frames();
  // Trace::beginSection("num_frames %d, underruns %d, buffer size %d",
  //                    num_frames, underrun_count, buffer_size);

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioPlayer::OnMessage(rtc::Message* msg) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  switch (msg->message_id) {
    case kMessageOutputStreamDisconnected:
      HandleStreamDisconnected();
      break;
    default:
      RTC_LOG(LS_ERROR) << "Invalid message id: " << msg->message_id;
      break;
  }
}

void AAudioPlayer::HandleStreamDisconnected() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_LOG(INFO) << ">>>>> HandleStreamDisconnected";
  if (!initialized_ || !playing_) {
    return;
  }
  // TODO(henrika): improve comments...
  audio_device_buffer_->NativeAudioPlayoutInterrupted();
  StopPlayout();
  InitPlayout();
  StartPlayout();
  int old_value = rtc::AtomicOps::CompareAndSwap(&restart_count_, 1, 0);
  RTC_DCHECK_EQ(1, old_value);
  RTC_LOG(WARNING) << "___Restart of output is done: " << restart_count_;
}

}  // namespace webrtc
