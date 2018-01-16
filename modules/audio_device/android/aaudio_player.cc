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

// #include "api/array_view.h"
// #include "modules/audio_device/android/audio_common.h"
#include "modules/audio_device/android/audio_manager.h"
// #include "modules/audio_device/fine_audio_buffer.h"
// #include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
// #include "rtc_base/format_macros.h"
// #include "rtc_base/platform_thread.h"
// #include "rtc_base/timeutils.h"

#define LOG_ON_ERROR(op)                                 \
  do {                                                   \
    aaudio_result_t result = (op);                       \
    if (result == AAUDIO_OK) {                           \
      char buf[256];                                     \
      rtc::sprintfn(buf, sizeof(buf), "%s: %s", #op,     \
                    AAudio_convertResultToText(result)); \
      RTC_LOG(LS_ERROR) << buf;                          \
    }                                                    \
  } while (0)

namespace webrtc {

AAudioPlayer::AAudioPlayer(AudioManager* audio_manager)
    : audio_parameters_(audio_manager->GetPlayoutAudioParameters()),
      audio_device_buffer_(nullptr),
      initialized_(false),
      playing_(false) {
  RTC_LOG(INFO) << "ctor";
  RTC_LOG(INFO) << audio_parameters_.ToString();
  // Detach from this thread since we want to use the checker to verify calls
  // from the internal  audio thread.
  // thread_checker_opensles_.DetachFromThread();
}

AAudioPlayer::~AAudioPlayer() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

int AAudioPlayer::Init() {
  RTC_LOG(INFO) << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_parameters_.channels() == 2) {
    // TODO(henrika): FineAudioBuffer needs more work to support stereo.
    RTC_LOG(LS_ERROR) << "AAudioPlayer does not support stereo";
    return -1;
  }
  // REMOVE...
  builder_ = CreateStreamBuilder();
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
  builder_ = CreateStreamBuilder();
  return 0;
}

int AAudioPlayer::StartPlayout() {
  RTC_LOG(INFO) << "StartPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!playing_);
  return 0;
}

int AAudioPlayer::StopPlayout() {
  RTC_LOG(INFO) << "StopPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !playing_) {
    return 0;
  }
  // Free resources for a stream created by AAudioStreamBuilder_openStream().
  LOG_ON_ERROR(AAudioStream_close(stream_));
  // Delete resources associated with the StreamBuilder.
  LOG_ON_ERROR(AAudioStreamBuilder_delete(builder_));
  initialized_ = false;
  playing_ = false;
  return 0;
}

void AAudioPlayer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  RTC_LOG(INFO) << "AttachAudioBuffer";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const int sample_rate_hz = audio_parameters_.sample_rate();
  // ALOGD("SetPlayoutSampleRate(%d)", sample_rate_hz);
  audio_device_buffer_->SetPlayoutSampleRate(sample_rate_hz);
  const size_t channels = audio_parameters_.channels();
  // ALOGD("SetPlayoutChannels(%" PRIuS ")", channels);
  audio_device_buffer_->SetPlayoutChannels(channels);
  RTC_CHECK(audio_device_buffer_);
  // AllocateDataBuffers();
}

AAudioStreamBuilder* AAudioPlayer::CreateStreamBuilder() {
  RTC_LOG(INFO) << "CreateStreamBuilder";
  AAudioStreamBuilder* builder = nullptr;
  LOG_ON_ERROR(AAudio_createStreamBuilder(&builder));
  /*
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK) {
    RTC_LOG(LS_ERROR) << "AAudio_createStreamBuilder failed: "
                      << AAudio_convertResultToText(result);
  } */
  return builder;
}

}  // namespace webrtc
