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
    if (result != AAUDIO_OK) {                           \
      char buf[256];                                     \
      rtc::sprintfn(buf, sizeof(buf), "%s: %s", #op,     \
                    AAudio_convertResultToText(result)); \
      RTC_LOG(LS_ERROR) << buf;                          \
    }                                                    \
  } while (0)

#define RETURN_ON_ERROR(op, ...)                         \
  do {                                                   \
    aaudio_result_t result = (op);                       \
    if (result != AAUDIO_OK) {                           \
      char buf[256];                                     \
      rtc::sprintfn(buf, sizeof(buf), "%s: %s", #op,     \
                    AAudio_convertResultToText(result)); \
      RTC_LOG(LS_ERROR) << buf;                          \
      return __VA_ARGS__;                                \
    }                                                    \
  } while (0)

namespace webrtc {

namespace {

/*
static void LogStreamState(AAudioStream* stream) {
  if (!stream)
    return;
  aaudio_stream_state_t currentState = AAudioStream_getState(stream);
  RTC_LOG(INFO) << "AAudio stream state: "
                << AAudio_convertStreamStateToText(currentState);
}*/

void ErrorCallback(AAudioStream* stream,
                   void* user_data,
                   aaudio_result_t error) {
  RTC_DCHECK(user_data);
  AAudioPlayer* aaudio_player = reinterpret_cast<AAudioPlayer*>(user_data);
  aaudio_player->OnErrorCallback(stream, error);
}

aaudio_data_callback_result_t DataCallback(AAudioStream* stream,
                                           void* user_data,
                                           void* audio_data,
                                           int32_t num_frames) {
  RTC_DCHECK(user_data);
  RTC_DCHECK(audio_data);
  AAudioPlayer* aaudio_player = reinterpret_cast<AAudioPlayer*>(user_data);
  return aaudio_player->OnDataCallback(stream, audio_data, num_frames);
}

}  // namespace

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
  RTC_DCHECK(!builder_);
  RTC_DCHECK(!stream_);
}

int AAudioPlayer::Init() {
  RTC_LOG(INFO) << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_parameters_.channels() == 2) {
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
  // TODO(henrika): move to common place once recording is supported as well.
  builder_ = CreateStreamBuilder();
  if (builder_ != nullptr) {
    SetAudioStreamConfiguration();
  }

  RETURN_ON_ERROR(AAudioStreamBuilder_openStream(builder_, &stream_), -1);
  RTC_DCHECK(stream_);

  // Maximum number of frames that can be filled without blocking.
  RTC_LOG(INFO) << "buffer size in frames: "
                << AAudioStream_getBufferSizeInFrames(stream_);
  // Maximum buffer capacity in frames.
  RTC_LOG(INFO) << "buffer capacity in frames: "
                << AAudioStream_getBufferCapacityInFrames(stream_);
  // Number of frames we should write at one time for optimal performance.
  int32_t frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  RTC_LOG(INFO) << "frames per burst: " << frames_per_burst;

  AAudioStream_setBufferSizeInFrames(stream_, frames_per_burst);

  RTC_LOG(INFO) << "buffer size in frames: "
                << AAudioStream_getBufferSizeInFrames(stream_);

  initialized_ = (builder_ != nullptr && stream_ != nullptr);
  return 0;
}

int AAudioPlayer::StartPlayout() {
  RTC_LOG(INFO) << "StartPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!playing_);
  RTC_DCHECK(builder_);

  playing_ = true;
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
  stream_ = nullptr;
  DeleteStreamBuilder();
  initialized_ = false;
  playing_ = false;
  return 0;
}

void AAudioPlayer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  RTC_LOG(INFO) << "AttachAudioBuffer";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  audio_device_buffer_->SetPlayoutSampleRate(audio_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(audio_parameters_.channels());
  RTC_CHECK(audio_device_buffer_);
  // AllocateDataBuffers();
}

AAudioStreamBuilder* AAudioPlayer::CreateStreamBuilder() {
  RTC_LOG(INFO) << "CreateStreamBuilder";
  AAudioStreamBuilder* builder = nullptr;
  LOG_ON_ERROR(AAudio_createStreamBuilder(&builder));
  return builder;
}

void AAudioPlayer::DeleteStreamBuilder() {
  RTC_LOG(INFO) << "DeleteStreamBuilder";
  RTC_DCHECK(builder_);
  LOG_ON_ERROR(AAudioStreamBuilder_delete(builder_));
  builder_ = nullptr;
}

void AAudioPlayer::SetAudioStreamConfiguration() {
  RTC_LOG(INFO) << "SetAudioStreamConfiguration";
  // Request usage of default primary output device.
  // TODO(henrika): use AAudioStream_getDeviceId() to verify on the stream.
  // TODO(henrika): verify that default device follows Java APIs.
  // https://developer.android.com/reference/android/media/AudioDeviceInfo.html.
  AAudioStreamBuilder_setDeviceId(builder_, device_id_);
  AAudioStreamBuilder_setSampleRate(builder_, audio_parameters_.sample_rate());
  AAudioStreamBuilder_setChannelCount(builder_, audio_parameters_.channels());
  AAudioStreamBuilder_setChannelCount(builder_, AAUDIO_FORMAT_PCM_I16);
  // Compare with AAUDIO_SHARING_MODE_EXCLUSIVE.
  // AAUDIO_SHARING_MODE_SHARED
  AAudioStreamBuilder_setSharingMode(builder_, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setDirection(builder_, AAUDIO_DIRECTION_OUTPUT);
  // Compare with AAUDIO_PERFORMANCE_MODE_NONE.
  // TODO(henrika): add comments...
  AAudioStreamBuilder_setPerformanceMode(builder_,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  // AAudioStreamBuilder_setBufferCapacityInFrames()
  AAudioStreamBuilder_setErrorCallback(builder_, ErrorCallback, this);
  AAudioStreamBuilder_setDataCallback(builder_, DataCallback, this);
}

// TODO(henrika): improve (possibly restart)...
void AAudioPlayer::OnErrorCallback(AAudioStream* stream,
                                   aaudio_result_t error) {
  RTC_LOG(LS_ERROR) << "OnErrorCallback: " << AAudio_convertResultToText(error);
}

// Render and write |num_frames| into |audio_data|.
aaudio_data_callback_result_t AAudioPlayer::OnDataCallback(AAudioStream* stream,
                                                           void* audio_data,
                                                           int32_t num_frames) {
  RTC_DCHECK_EQ(stream, stream_);
  RTC_LOG(INFO) << "OnDataCallback: " << num_frames;
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

}  // namespace webrtc
