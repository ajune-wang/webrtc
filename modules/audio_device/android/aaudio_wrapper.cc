/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/aaudio_wrapper.h"

#include "modules/audio_device/android/audio_manager.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"

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
// TODO(henrika): currently only contains IDs for sinks (output). Will add
// IDs for sources (inputs) as well.
enum DeviceType {
  DEFAULT = AAUDIO_UNSPECIFIED,
  BUILTIN_EARPIECE = 1,
  BUILTIN_SPEAKER = 2,
  TELEPHONY = 6,
  WIRED_HEADSET = 264,
};

const char* DirectionToString(aaudio_direction_t direction) {
  switch (direction) {
    case AAUDIO_DIRECTION_OUTPUT:
      return "OUTPUT";
    case AAUDIO_DIRECTION_INPUT:
      return "INPUT";
    default:
      return "UNKOWN";
  }
}

const char* SharingModeToString(aaudio_sharing_mode_t mode) {
  switch (mode) {
    case AAUDIO_SHARING_MODE_EXCLUSIVE:
      return "EXCLUSIVE";
    case AAUDIO_SHARING_MODE_SHARED:
      return "SHARED";
    default:
      return "UNKOWN";
  }
}

const char* PerformanceModeToString(aaudio_performance_mode_t mode) {
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
  AAudioWrapper* aaudio_wrapper = reinterpret_cast<AAudioWrapper*>(user_data);
  RTC_DCHECK(aaudio_wrapper->observer());
  aaudio_wrapper->observer()->OnErrorCallback(error);
}

aaudio_data_callback_result_t DataCallback(AAudioStream* stream,
                                           void* user_data,
                                           void* audio_data,
                                           int32_t num_frames) {
  RTC_DCHECK(user_data);
  RTC_DCHECK(audio_data);
  AAudioWrapper* aaudio_wrapper = reinterpret_cast<AAudioWrapper*>(user_data);
  RTC_DCHECK(aaudio_wrapper->observer());
  return aaudio_wrapper->observer()->OnDataCallback(audio_data, num_frames);
}

}  // namespace

AAudioWrapper::AAudioWrapper(AudioManager* audio_manager,
                             aaudio_direction_t direction,
                             AAudioObserverInterface* observer)
    : direction_(direction), observer_(observer) {
  RTC_DCHECK(observer_);
  direction_ == AAUDIO_DIRECTION_OUTPUT
      ? audio_parameters_ = audio_manager->GetPlayoutAudioParameters()
      : audio_parameters_ = audio_manager->GetRecordAudioParameters();
  RTC_LOG(INFO) << "ctor";
  RTC_LOG(INFO) << audio_parameters_.ToString();
}

AAudioWrapper::~AAudioWrapper() {
  RTC_LOG(INFO) << "dtor";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!builder_);
  RTC_DCHECK(!stream_);
}

bool AAudioWrapper::Init() {
  RTC_LOG(INFO) << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Creates a stream builder which can be used to open an audio stream.
  if (!CreateStreamBuilder()) {
    return false;
  }
  // Configures the stream builder using audio parameters given at construction.
  SetStreamConfiguration();
  // Opens a stream based on options in the stream builder.
  if (!OpenStream()) {
    return false;
  }
  // Ensures that the opened stream could activate the requested settings.
  if (!VerifyStreamConfiguration()) {
    return false;
  }
  // Optimizes the buffer scheme for lowest possible latency and creates
  // additional buffer logic to match the 10ms buffer size used in WebRTC.
  if (!OptimizeBuffers()) {
    return false;
  }
  LogStreamState();
  return true;
}

bool AAudioWrapper::Start() {
  RTC_LOG(INFO) << "Start";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(henrika): this state check might not be needed.
  aaudio_stream_state_t current_state = AAudioStream_getState(stream_);
  if (current_state != AAUDIO_STREAM_STATE_OPEN) {
    RTC_LOG(LS_ERROR) << "Invalid state: "
                      << AAudio_convertStreamStateToText(current_state);
    return false;
  }
  // Asynchronous request for the stream to start.
  RETURN_ON_ERROR(AAudioStream_requestStart(stream_), false);
  LogStreamState();
  return true;
}

bool AAudioWrapper::Stop() {
  RTC_LOG(INFO) << "Stop";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Asynchronous request for the stream to stop.
  RETURN_ON_ERROR(AAudioStream_requestStop(stream_), false);
  // Releases all acquired resources (for the builder and the stream).
  CloseStream();
  DeleteStreamBuilder();
  return true;
}

AAudioObserverInterface* AAudioWrapper::observer() const {
  return observer_;
}

AudioParameters AAudioWrapper::audio_parameters() const {
  return audio_parameters_;
}

int32_t AAudioWrapper::samples_per_frame() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getSamplesPerFrame(stream_);
}

int32_t AAudioWrapper::buffer_size_in_frames() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getBufferSizeInFrames(stream_);
}

int32_t AAudioWrapper::device_id() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getDeviceId(stream_);
}

int32_t AAudioWrapper::xrun_count() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getXRunCount(stream_);
}

int32_t AAudioWrapper::format() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getFormat(stream_);
}

int32_t AAudioWrapper::sample_rate() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getSampleRate(stream_);
}

int32_t AAudioWrapper::channel_count() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getChannelCount(stream_);
}

int32_t AAudioWrapper::frames_per_callback() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getFramesPerDataCallback(stream_);
}

aaudio_sharing_mode_t AAudioWrapper::sharing_mode() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getSharingMode(stream_);
}

aaudio_performance_mode_t AAudioWrapper::performance_mode() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getPerformanceMode(stream_);
}

aaudio_direction_t AAudioWrapper::direction() const {
  RTC_DCHECK(stream_);
  aaudio_performance_mode_t direction = AAudioStream_getDirection(stream_);
  RTC_CHECK_EQ(direction, direction_);
  return direction;
}

bool AAudioWrapper::CreateStreamBuilder() {
  RTC_LOG(INFO) << "CreateStreamBuilder";
  AAudioStreamBuilder* builder = nullptr;
  RETURN_ON_ERROR(AAudio_createStreamBuilder(&builder), false);
  builder_ = builder;
  return true;
}

void AAudioWrapper::DeleteStreamBuilder() {
  RTC_LOG(INFO) << "DeleteStreamBuilder";
  RTC_DCHECK(builder_);
  LOG_ON_ERROR(AAudioStreamBuilder_delete(builder_));
  builder_ = nullptr;
}

void AAudioWrapper::SetStreamConfiguration() {
  RTC_LOG(INFO) << "SetStreamConfiguration";
  RTC_DCHECK(builder_);
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Request usage of default primary output/input device.
  // TODO(henrika): verify that default device follows Java APIs.
  // https://developer.android.com/reference/android/media/AudioDeviceInfo.html.
  AAudioStreamBuilder_setDeviceId(builder_, DeviceType::DEFAULT);
  // Use preferred sample rate given by the audio parameters.
  AAudioStreamBuilder_setSampleRate(builder_, audio_parameters().sample_rate());
  // Use preferred channel configuration given by the audio parameters.
  AAudioStreamBuilder_setChannelCount(builder_, audio_parameters().channels());
  // Always use 16-bit PCM audio sample format.
  AAudioStreamBuilder_setFormat(builder_, AAUDIO_FORMAT_PCM_I16);
  // TODO(henrika): cost likely AAUDIO_SHARING_MODE_EXCLUSIVE can't be used.
  AAudioStreamBuilder_setSharingMode(builder_, AAUDIO_SHARING_MODE_SHARED);
  // Use the direction that was given at construction.
  AAudioStreamBuilder_setDirection(builder_, direction_);
  // TODO(henrika): investigate performance using different performance modes.
  AAudioStreamBuilder_setPerformanceMode(builder_,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  // Use callback running on a real-time and high-priority thread to move audio.
  // The stream will periodically execute the callback function to acquire data
  // for the next burst.
  AAudioStreamBuilder_setDataCallback(builder_, DataCallback, this);
  // Request that AAudio calls this functions if any error occurs on a callback
  // thread.
  AAudioStreamBuilder_setErrorCallback(builder_, ErrorCallback, this);
}

bool AAudioWrapper::OpenStream() {
  RTC_LOG(INFO) << "OpenStream";
  RTC_DCHECK(builder_);
  AAudioStream* stream = nullptr;
  RETURN_ON_ERROR(AAudioStreamBuilder_openStream(builder_, &stream), false);
  stream_ = stream;
  LogStreamConfiguration();
  return true;
}

void AAudioWrapper::CloseStream() {
  RTC_LOG(INFO) << "CloseStream";
  RTC_DCHECK(stream_);
  LOG_ON_ERROR(AAudioStream_close(stream_));
  stream_ = nullptr;
}

void AAudioWrapper::LogStreamConfiguration() {
  RTC_DCHECK(stream_);
  std::ostringstream os;
  os << "Stream Configuration: "
     << "sample rate=" << sample_rate() << ", channels=" << channel_count()
     << ", samples per frame=" << samples_per_frame()
     << ", format=" << FormatToString(format())
     << ", sharing mode=" << SharingModeToString(sharing_mode())
     << ", performance mode=" << PerformanceModeToString(performance_mode())
     << ", direction=" << DirectionToString(AAudioStream_getDirection(stream_))
     << ", device id=" << DeviceIdToString(AAudioStream_getDeviceId(stream_))
     << ", frames per callback=" << frames_per_callback();
  RTC_LOG(INFO) << os.str();
}

void AAudioWrapper::LogStreamState() {
  RTC_DCHECK(stream_);
  aaudio_stream_state_t currentState = AAudioStream_getState(stream_);
  RTC_LOG(INFO) << "AAudio stream state: "
                << AAudio_convertStreamStateToText(currentState);
}

bool AAudioWrapper::VerifyStreamConfiguration() {
  RTC_LOG(INFO) << "VerifyStreamConfiguration";
  RTC_DCHECK(stream_);
  // TODO(henrika): should we verify device ID as well?
  if (AAudioStream_getSampleRate(stream_) != audio_parameters().sample_rate()) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
    return false;
  }
  if (AAudioStream_getChannelCount(stream_) !=
      static_cast<int32_t>(audio_parameters().channels())) {
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
      static_cast<int32_t>(audio_parameters().channels())) {
    RTC_LOG(LS_ERROR) << "Invalid number of samples per frame";
    return false;
  }
  return true;
}

bool AAudioWrapper::OptimizeBuffers() {
  RTC_LOG(INFO) << "OptimizeBuffers";
  RTC_DCHECK(stream_);
  RTC_LOG(INFO) << "max buffer capacity in frames: "
                << AAudioStream_getBufferCapacityInFrames(stream_);
  int32_t frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  RTC_LOG(INFO) << "frames per burst for optimal performance: "
                << frames_per_burst;
  // Set buffer size to same as burst size to guarantee lowest possible latency.
  // This size might change for output streams if underruns are detected and
  // automatic buffer adjustment is enabled.
  AAudioStream_setBufferSizeInFrames(stream_, frames_per_burst);
  int32_t buffer_size = AAudioStream_getBufferSizeInFrames(stream_);
  if (buffer_size != frames_per_burst) {
    RTC_LOG(LS_ERROR) << "Failed to use optimal buffer burst size";
    return false;
  }
  // Maximum number of frames that can be filled without blocking.
  RTC_LOG(INFO) << "buffer burst size in frames: " << buffer_size;
  return true;
}

}  // namespace webrtc
