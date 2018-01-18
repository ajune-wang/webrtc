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
// #include "modules/audio_device/android/audio_common.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/fine_audio_buffer.h"
// #include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
// #include "rtc_base/format_macros.h"
// #include "rtc_base/platform_thread.h"
#include "rtc_base/timeutils.h"

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

// https://developer.android.com/reference/android/media/AudioDeviceInfo.html#getId()
// Use IDs for sinks only (output).
enum DeviceType {
  DEFAULT = AAUDIO_UNSPECIFIED,
  BUILTIN_EARPIECE = 1,
  BUILTIN_SPEAKER = 2,
  TELEPHONY = 6,
  WIRED_HEADSET = 264,
};

const char* DirectionToString(int32_t direction) {
  switch (direction) {
    case AAUDIO_DIRECTION_OUTPUT:
      return "OUTPUT";
    case AAUDIO_DIRECTION_INPUT:
      return "INPUT";
    default:
      return "UNKOWN";
  }
}

const char* SharingModeToString(int32_t mode) {
  switch (mode) {
    case AAUDIO_SHARING_MODE_EXCLUSIVE:
      return "EXCLUSIVE";
    case AAUDIO_SHARING_MODE_SHARED:
      return "SHARED";
    default:
      return "UNKOWN";
  }
}

const char* PerformanceModeToString(int32_t mode) {
  switch (mode) {
    case AAUDIO_PERFORMANCE_MODE_NONE:
      return "NONE";
    case AAUDIO_PERFORMANCE_MODE_POWER_SAVING:
      return "POWER_SAVING";
    case AAUDIO_PERFORMANCE_MODE_LOW_LATENCY:
      return "LOW_LATENCY";
    default:
      return "UNKOWN";
  }
}

const char* FormatToString(int32_t id) {
  switch (id) {
    case AAUDIO_FORMAT_INVALID:
      return "INVALID";
    case AAUDIO_FORMAT_UNSPECIFIED:
      return "UNSPECIFIED";
    case AAUDIO_FORMAT_PCM_I16:
      return "PCM_I16";
    case AAUDIO_FORMAT_PCM_FLOAT:
      return "FLOAT";
    default:
      return "UNKOWN";
  }
}

const char* DeviceIdToString(int32_t format) {
  switch (format) {
    case DeviceType::BUILTIN_EARPIECE:
      return "BUILTIN_EARPIECE";
    case DeviceType::BUILTIN_SPEAKER:
      return "BUILTIN_SPEAKER";
    case DeviceType::TELEPHONY:
      return "TELEPHONY";
    case DeviceType::WIRED_HEADSET:
      return "WIRED_HEADSET";
    default:
      return "UNKOWN";
  }
}

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
      playing_(false),
      device_id_(DeviceType::DEFAULT) {
  RTC_LOG(INFO) << "ctor";
  RTC_LOG(INFO) << audio_parameters_.ToString();
  // Detach from this thread since we want to use the checker to verify calls
  // from the real-time thread owned by AAudio.
  thread_checker_aaudio_.DetachFromThread();
}

AAudioPlayer::~AAudioPlayer() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  RTC_DCHECK(!builder_);
  RTC_DCHECK(!stream_);
  RTC_LOG(INFO) << "detected underruns: " << underrun_count_;
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
  // TODO(henrika): fix return values.
  CreateStreamBuilder();
  SetStreamConfiguration();
  OpenStream();
  VerifyStreamConfiguration();
  OptimizeBuffers();
  LogStreamState();
  initialized_ = true;
  return 0;
}

int AAudioPlayer::StartPlayout() {
  RTC_LOG(INFO) << "StartPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!playing_);
  RTC_DCHECK(stream_);
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }
  // TODO(henrika): this state check might not be needed.
  aaudio_stream_state_t current_state = AAudioStream_getState(stream_);
  if (current_state != AAUDIO_STREAM_STATE_OPEN) {
    RTC_LOG(LS_ERROR) << "Invalid state: "
                      << AAudio_convertStreamStateToText(current_state);
    return -1;
  }
  last_play_time_ = rtc::Time();
  // Asynchronous request for the stream to start.
  RETURN_ON_ERROR(AAudioStream_requestStart(stream_), -1);
  // Track underrun count for statistics and automatic buffer adjustments.
  underrun_count_ = AAudioStream_getXRunCount(stream_);
  LogStreamState();
  playing_ = true;
  return 0;
}

int AAudioPlayer::StopPlayout() {
  RTC_LOG(INFO) << "StopPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !playing_) {
    return 0;
  }
  // Asynchronous request for the stream to stop.
  RETURN_ON_ERROR(AAudioStream_requestStop(stream_), -1);
  CloseStream();
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
  // Create a modified audio buffer class which allows us to ask for any number
  // of samples (and not only multiple of 10ms) to match the native AAudio
  // buffer size buffer_size_in_frames_ which will be set in OptimizeBuffers().
  const size_t capacity = 2 * audio_parameters_.GetBytesPer10msBuffer();
  fine_audio_buffer_.reset(new FineAudioBuffer(
      audio_device_buffer_, audio_parameters_.sample_rate(), capacity));
}

bool AAudioPlayer::CreateStreamBuilder() {
  RTC_LOG(INFO) << "CreateStreamBuilder";
  AAudioStreamBuilder* builder = nullptr;
  RETURN_ON_ERROR(AAudio_createStreamBuilder(&builder), false);
  builder_ = builder;
  return true;
}

void AAudioPlayer::DeleteStreamBuilder() {
  RTC_LOG(INFO) << "DeleteStreamBuilder";
  RTC_DCHECK(builder_);
  LOG_ON_ERROR(AAudioStreamBuilder_delete(builder_));
  builder_ = nullptr;
}

bool AAudioPlayer::OpenStream() {
  RTC_LOG(INFO) << "OpenStream";
  RTC_DCHECK(builder_);
  AAudioStream* stream = nullptr;
  RETURN_ON_ERROR(AAudioStreamBuilder_openStream(builder_, &stream), false);
  stream_ = stream;
  LogStreamConfiguration();
  return true;
}

void AAudioPlayer::CloseStream() {
  RTC_LOG(INFO) << "CloseStream";
  RTC_DCHECK(stream_);
  LOG_ON_ERROR(AAudioStream_close(stream_));
  stream_ = nullptr;
}

void AAudioPlayer::SetStreamConfiguration() {
  RTC_LOG(INFO) << "SetStreamConfiguration";
  RTC_DCHECK(builder_);
  // Request usage of default primary output device.
  // TODO(henrika): use AAudioStream_getDeviceId() to verify on the stream.
  // TODO(henrika): verify that default device follows Java APIs.
  // https://developer.android.com/reference/android/media/AudioDeviceInfo.html.
  AAudioStreamBuilder_setDeviceId(builder_, device_id_);
  AAudioStreamBuilder_setSampleRate(builder_, audio_parameters_.sample_rate());
  AAudioStreamBuilder_setChannelCount(builder_, audio_parameters_.channels());
  AAudioStreamBuilder_setFormat(builder_, AAUDIO_FORMAT_PCM_I16);
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
  // TODO(henrika): figure out if we should use 10ms buffers in native AAudio
  // or adapt using a local FineAudioBuffer.
  // AAudioStreamBuilder_setFramesPerDataCallback(
  //    builder_, audio_parameters_.frames_per_10ms_buffer());
}

void AAudioPlayer::LogStreamConfiguration() {
  RTC_DCHECK(stream_);
  std::ostringstream os;
  os << "Stream Configuration: "
     << "sample rate=" << AAudioStream_getSampleRate(stream_)
     << ", channels=" << AAudioStream_getChannelCount(stream_)
     << ", samples per frame=" << AAudioStream_getSamplesPerFrame(stream_)
     << ", format=" << FormatToString(AAudioStream_getFormat(stream_))
     << ", sharing mode="
     << SharingModeToString(AAudioStream_getSharingMode(stream_))
     << ", performance mode="
     << PerformanceModeToString(AAudioStream_getPerformanceMode(stream_))
     << ", direction=" << DirectionToString(AAudioStream_getDirection(stream_))
     << ", device id=" << DeviceIdToString(AAudioStream_getDeviceId(stream_))
     << ", frames per callback="
     << AAudioStream_getFramesPerDataCallback(stream_);
  RTC_LOG(INFO) << os.str();
}

void AAudioPlayer::LogStreamState() {
  RTC_DCHECK(stream_);
  aaudio_stream_state_t currentState = AAudioStream_getState(stream_);
  RTC_LOG(INFO) << "AAudio stream state: "
                << AAudio_convertStreamStateToText(currentState);
}

bool AAudioPlayer::VerifyStreamConfiguration() {
  RTC_LOG(INFO) << "VerifyStreamConfiguration";
  RTC_DCHECK(stream_);
  // TODO(henrika): how to check for default?
  // if (AAudioStream_getDeviceId(stream_) != device_id_) {
  //   RTC_LOG(LS_ERROR) << "Stream unable to use requested audio device";
  //   return false;
  // }
  if (AAudioStream_getSampleRate(stream_) != audio_parameters_.sample_rate()) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
    return false;
  }
  if (AAudioStream_getChannelCount(stream_) !=
      static_cast<int32_t>(audio_parameters_.channels())) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested channel count";
    return false;
  }
  if (AAudioStream_getFormat(stream_) != AAUDIO_FORMAT_PCM_I16) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested format";
    return false;
  }
  if (AAudioStream_getSharingMode(stream_) != AAUDIO_SHARING_MODE_SHARED) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested sharing mode";
    return false;
  }
  if (AAudioStream_getPerformanceMode(stream_) !=
      AAUDIO_PERFORMANCE_MODE_LOW_LATENCY) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested performance mode";
    return false;
  }
  if (AAudioStream_getDirection(stream_) != AAUDIO_DIRECTION_OUTPUT) {
    RTC_LOG(LS_ERROR) << "Stream direction could not be set to output";
    return false;
  }
  if (AAudioStream_getSamplesPerFrame(stream_) !=
      static_cast<int32_t>(audio_parameters_.channels())) {
    RTC_LOG(LS_ERROR) << "Invalid number of samples per frame";
    return false;
  }
  return true;
}

bool AAudioPlayer::OptimizeBuffers() {
  RTC_LOG(INFO) << "OptimizeBuffers";
  RTC_DCHECK(stream_);
  RTC_LOG(INFO) << "max buffer capacity in frames: "
                << AAudioStream_getBufferCapacityInFrames(stream_);
  int32_t frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  RTC_LOG(INFO) << "frames per burst for optimal performance: "
                << frames_per_burst;
  // Set buffer size to same as burst size to guarantee lowest possible latency.
  AAudioStream_setBufferSizeInFrames(stream_, frames_per_burst);
  int32_t buffer_size = AAudioStream_getBufferSizeInFrames(stream_);
  if (buffer_size != frames_per_burst) {
    RTC_LOG(LS_ERROR) << "Failed to use optimal buffer size";
    return false;
  }
  // Maximum number of frames that can be filled without blocking.
  RTC_LOG(INFO) << "buffer size in frames: " << buffer_size;
  buffer_size_in_frames_ = buffer_size;
  return true;
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
  RTC_DCHECK(thread_checker_aaudio_.CalledOnValidThread());
  RTC_LOG(INFO) << "OnDataCallback: " << num_frames;

  // const uint32_t current_time = rtc::Time();
  // const uint32_t diff = current_time - last_play_time_;
  // last_play_time_ = current_time;
  // RTC_LOG(INFO) << "dt: " << diff;

  // TODO(henrika): implement buffer adjustment here by increasing buffer size
  // with one burst.
  const int32_t underrun_count = AAudioStream_getXRunCount(stream_);
  if (underrun_count > underrun_count_) {
    RTC_LOG(LS_ERROR) << "Underrun detected: " << underrun_count;
    underrun_count_ = underrun_count;
  }

  const int32_t samples_per_frame = AAudioStream_getSamplesPerFrame(stream_);
  const size_t size_in_bytes = sizeof(int16_t) * samples_per_frame * num_frames;
  // Read audio data from the WebRTC source using the FineAudioBuffer object
  // to adjust for differences in buffer size between WebRTC (10ms) and native
  // AAudio
  fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int8_t>(static_cast<int8_t*>(audio_data), size_in_bytes));

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
