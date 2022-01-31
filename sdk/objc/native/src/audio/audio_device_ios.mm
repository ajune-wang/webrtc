/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "audio_device_ios.h"

#include <array>
#include <cmath>

#include "api/array_view.h"
#include "helpers.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"
#import "components/audio/RTCAudioSession+Private.h"
#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"
#import "components/audio/RTCNativeAudioSessionDelegateAdapter.h"

namespace webrtc {
namespace ios_adm {

#define LOGI() RTC_LOG(LS_INFO) << "AudioDeviceIOS::"

#define LOG_AND_RETURN_IF_ERROR(error, message)    \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
      return false;                                \
    }                                              \
  } while (0)

#define LOG_IF_ERROR(error, message)               \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
    }                                              \
  } while (0)

// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

enum AudioDeviceMessageType : uint32_t {
  kMessageTypeInterruptionBegin,
  kMessageTypeInterruptionEnd,
  kMessageTypeValidRouteChange,
  kMessageTypeCanPlayOrRecordChange,
  kMessageTypePlayoutGlitchDetected,
  kMessageOutputVolumeChange,
};

using ios::CheckAndLogError;

#if !defined(NDEBUG)
// Returns true when the code runs on a device simulator.
static bool DeviceIsSimulator() {
  return ios::GetDeviceName() == "x86_64";
}

// Helper method that logs essential device information strings.
static void LogDeviceInfo() {
  RTC_LOG(LS_INFO) << "LogDeviceInfo";
  @autoreleasepool {
    RTC_LOG(LS_INFO) << " system name: " << ios::GetSystemName();
    RTC_LOG(LS_INFO) << " system version: " << ios::GetSystemVersionAsString();
    RTC_LOG(LS_INFO) << " device type: " << ios::GetDeviceType();
    RTC_LOG(LS_INFO) << " device name: " << ios::GetDeviceName();
    RTC_LOG(LS_INFO) << " process name: " << ios::GetProcessName();
    RTC_LOG(LS_INFO) << " process ID: " << ios::GetProcessID();
    RTC_LOG(LS_INFO) << " OS version: " << ios::GetOSVersionString();
    RTC_LOG(LS_INFO) << " processing cores: " << ios::GetProcessorCount();
    RTC_LOG(LS_INFO) << " low power mode: " << ios::GetLowPowerModeEnabled();
#if TARGET_IPHONE_SIMULATOR
    RTC_LOG(LS_INFO) << " TARGET_IPHONE_SIMULATOR is defined";
#endif
    RTC_LOG(LS_INFO) << " DeviceIsSimulator: " << DeviceIsSimulator();
  }
}
#endif  // !defined(NDEBUG)

AudioDeviceIOS::AudioDeviceIOS(bool bypass_voice_processing)
    : bypass_voice_processing_(bypass_voice_processing),
      audio_device_buffer_(nullptr),
      audio_input_unit_(nullptr),
      audio_output_unit_(nullptr),
      recording_(0),
      playing_(0),
      initialized_(false),
      is_interrupted_(false),
      is_audio_session_subscribed_(false),
      has_configured_session_(false),
      num_detected_playout_glitches_(0),
      last_playout_time_(0),
      num_playout_callbacks_(0),
      last_output_volume_change_time_(0) {
  LOGI() << "ctor" << ios::GetCurrentThreadDescription()
         << ",bypass_voice_processing=" << bypass_voice_processing_;
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();
  thread_checker_.Detach();
  thread_ = rtc::Thread::Current();

  audio_session_observer_ = [[RTCNativeAudioSessionDelegateAdapter alloc] initWithObserver:this];
}

AudioDeviceIOS::~AudioDeviceIOS() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  LOGI() << "~dtor" << ios::GetCurrentThreadDescription();
  thread_->Clear(this);
  Terminate();
  audio_session_observer_ = nil;
}

void AudioDeviceIOS::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK(thread_checker_.IsCurrent());
  audio_device_buffer_ = audioBuffer;
}

AudioDeviceGeneric::InitStatus AudioDeviceIOS::Init() {
  LOGI() << "Init";
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();
  thread_checker_.Detach();

  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (initialized_) {
    return InitStatus::OK;
  }
#if !defined(NDEBUG)
  LogDeviceInfo();
#endif
  // Store the preferred sample rate and preferred number of channels already
  // here. They have not been set and confirmed yet since configureForWebRTC
  // is not called until audio is about to start. However, it makes sense to
  // store the parameters now and then verify at a later stage.
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* config =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  playout_parameters_.reset(config.sampleRate, config.outputNumberOfChannels);
  record_parameters_.reset(config.sampleRate, config.inputNumberOfChannels);
  // Ensure that the audio device buffer (ADB) knows about the internal audio
  // parameters. Note that, even if we are unable to get a mono audio session,
  // we will always tell the I/O audio unit to do a channel format conversion
  // to guarantee mono on the "input side" of the audio unit.
  UpdateAudioDeviceBuffer(TargetAudioUnit::kInput);
  UpdateAudioDeviceBuffer(TargetAudioUnit::kOutput);
  initialized_ = true;
  return InitStatus::OK;
}

int32_t AudioDeviceIOS::Terminate() {
  LOGI() << "Terminate";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!initialized_) {
    return 0;
  }
  StopPlayout();
  StopRecording();
  initialized_ = false;
  return 0;
}

bool AudioDeviceIOS::Initialized() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return initialized_;
}

int32_t AudioDeviceIOS::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(initialized_);
  RTC_DCHECK(audio_output_unit_ == nullptr);
  RTC_DCHECK(!playing_);
  if (audio_output_unit_ == nullptr) {
    if (!CreateAudioOutputUnit()) {
      RTC_LOG_F(LS_ERROR) << "CreateAudioOutputUnit failed for InitPlayout!";
      return -1;
    }
    bool can_play_or_record = true;
    if (!RecordingIsInitialized() && !InitializeAudioSession(can_play_or_record)) {
      RTCLogError(@"InitPlayout failed initialize audio session");
      audio_output_unit_.reset();
      return -1;
    }
    if (can_play_or_record) {
      SetupAudioBuffersForActiveAudioSession(TargetAudioUnit::kOutput);
      if (!audio_output_unit_->Initialize(playout_parameters_.sample_rate())) {
        RTCLogWarning(@"InitPlayout failed initialize audio output unit");
      }
    }
  }
  return 0;
}

bool AudioDeviceIOS::PlayoutIsInitialized() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return audio_output_unit_ != nullptr;
}

bool AudioDeviceIOS::RecordingIsInitialized() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return audio_input_unit_ != nullptr;
}

int32_t AudioDeviceIOS::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(initialized_);
  RTC_DCHECK(audio_input_unit_ == nullptr);
  RTC_DCHECK(!recording_);
  if (audio_input_unit_ == nullptr) {
    if (!CreateAudioInputUnit()) {
      RTC_LOG_F(LS_ERROR) << "CreateAudioInputUnit failed for InitRecording!";
      return -1;
    }
    bool can_play_or_record = true;
    if (!PlayoutIsInitialized() && !InitializeAudioSession(can_play_or_record)) {
      RTCLogError(@"StartRecording failed initialize audio session");
      audio_input_unit_.reset();
      return -1;
    }
    if (can_play_or_record) {
      SetupAudioBuffersForActiveAudioSession(TargetAudioUnit::kInput);
      if (!audio_input_unit_->Initialize(record_parameters_.sample_rate())) {
        RTCLogWarning(@"StartRecording failed initialize audio input unit");
      }
    }
  }
  return audio_input_unit_ != nullptr ? 0 : -1;
}

int32_t AudioDeviceIOS::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(audio_output_unit_ != nullptr);
  RTC_DCHECK(!playing_);
  if (playout_fine_audio_buffer_) {
    playout_fine_audio_buffer_->ResetPlayout();
  }
  if (audio_output_unit_->GetState() == BaseAudioUnit::State::kInitialized) {
    OSStatus result = audio_output_unit_->Start();
    if (result != noErr) {
      RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
      [session notifyAudioOutputUnitStartFailedWithError:result];
      RTCLogError(@"StartPlayout failed to start audio unit, reason %d", result);
      return -1;
    }
    RTC_LOG(LS_INFO) << "Audio output unit is now started";
  }
  rtc::AtomicOps::ReleaseStore(&playing_, 1);
  num_playout_callbacks_ = 0;
  num_detected_playout_glitches_ = 0;
  return 0;
}

int32_t AudioDeviceIOS::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!audio_output_unit_ || !playing_) {
    return 0;
  }
  rtc::AtomicOps::ReleaseStore(&playing_, 0);

  audio_output_unit_.reset();
  PrepareForNewStart(TargetAudioUnit::kOutput);
  if (!RecordingIsInitialized()) {
    Shutdown();
  }

  // Derive average number of calls to OnGetPlayoutData() between detected
  // audio glitches and add the result to a histogram.
  int average_number_of_playout_callbacks_between_glitches = 100000;
  RTC_DCHECK_GE(num_playout_callbacks_, num_detected_playout_glitches_);
  if (num_detected_playout_glitches_ > 0) {
    average_number_of_playout_callbacks_between_glitches =
        num_playout_callbacks_ / num_detected_playout_glitches_;
  }
  RTC_HISTOGRAM_COUNTS_100000("WebRTC.Audio.AveragePlayoutCallbacksBetweenGlitches",
                              average_number_of_playout_callbacks_between_glitches);
  RTCLog(@"Average number of playout callbacks between glitches: %d",
         average_number_of_playout_callbacks_between_glitches);
  return 0;
}

bool AudioDeviceIOS::Playing() const {
  return playing_;
}

int32_t AudioDeviceIOS::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(audio_input_unit_ != nullptr);
  RTC_DCHECK(!recording_);
  if (record_fine_audio_buffer_) {
    record_fine_audio_buffer_->ResetRecord();
  }
  if (audio_input_unit_->GetState() == BaseAudioUnit::State::kInitialized) {
    OSStatus result = audio_input_unit_->Start();
    if (result != noErr) {
      RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
      [session notifyAudioInputUnitStartFailedWithError:result];
      RTCLogError(@"StartRecording failed to start audio input unit, reason %d", result);
      return -1;
    }
    RTC_LOG(LS_INFO) << "Audio input unit is now started";
  }
  rtc::AtomicOps::ReleaseStore(&recording_, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (audio_input_unit_ == nullptr || !recording_) {
    return 0;
  }
  rtc::AtomicOps::ReleaseStore(&recording_, 0);
  audio_input_unit_.reset();
  PrepareForNewStart(TargetAudioUnit::kOutput);
  if (!PlayoutIsInitialized()) {
    Shutdown();
  }
  return 0;
}

bool AudioDeviceIOS::Recording() const {
  return recording_;
}

int32_t AudioDeviceIOS::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

int AudioDeviceIOS::GetPlayoutAudioParameters(AudioParameters* params) const {
  LOGI() << "GetPlayoutAudioParameters";
  RTC_DCHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.IsCurrent());
  *params = playout_parameters_;
  return 0;
}

int AudioDeviceIOS::GetRecordAudioParameters(AudioParameters* params) const {
  LOGI() << "GetRecordAudioParameters";
  RTC_DCHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.IsCurrent());
  *params = record_parameters_;
  return 0;
}

void AudioDeviceIOS::OnInterruptionBegin() {
  RTC_DCHECK(thread_);
  LOGI() << "OnInterruptionBegin";
  thread_->Post(RTC_FROM_HERE, this, kMessageTypeInterruptionBegin);
}

void AudioDeviceIOS::OnInterruptionEnd() {
  RTC_DCHECK(thread_);
  LOGI() << "OnInterruptionEnd";
  thread_->Post(RTC_FROM_HERE, this, kMessageTypeInterruptionEnd);
}

void AudioDeviceIOS::OnValidRouteChange() {
  RTC_DCHECK(thread_);
  thread_->Post(RTC_FROM_HERE, this, kMessageTypeValidRouteChange);
}

void AudioDeviceIOS::OnCanPlayOrRecordChange(bool can_play_or_record) {
  RTC_DCHECK(thread_);
  thread_->Post(RTC_FROM_HERE,
                this,
                kMessageTypeCanPlayOrRecordChange,
                new rtc::TypedMessageData<bool>(can_play_or_record));
}

void AudioDeviceIOS::OnChangedOutputVolume() {
  RTC_DCHECK(thread_);
  thread_->Post(RTC_FROM_HERE, this, kMessageOutputVolumeChange);
}

OSStatus AudioDeviceIOS::OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                               const AudioTimeStamp* time_stamp,
                                               UInt32 bus_number,
                                               UInt32 num_frames,
                                               AudioBufferList* /* io_data */) {
  RTC_DCHECK_RUN_ON(&io_record_thread_checker_);
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!rtc::AtomicOps::AcquireLoad(&recording_)) return result;

  // Set the size of our own audio buffer and clear it first to avoid copying
  // in combination with potential reallocations.
  // On real iOS devices, the size will only be set once (at first callback).
  record_audio_buffer_.Clear();
  record_audio_buffer_.SetSize(num_frames);

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer
  // in `record_audio_buffer_`. Recorded audio will be rendered into this memory
  // at each input callback when calling AudioUnitRender().
  AudioBufferList audio_buffer_list;
  audio_buffer_list.mNumberBuffers = 1;
  AudioBuffer* audio_buffer = &audio_buffer_list.mBuffers[0];
  audio_buffer->mNumberChannels = record_parameters_.channels();
  audio_buffer->mDataByteSize = record_audio_buffer_.size() * AudioInputUnit::kBytesPerSample;
  audio_buffer->mData = reinterpret_cast<int8_t*>(record_audio_buffer_.data());

  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the `io_data` parameter is a reference
  // to the preallocated audio buffer list that the audio unit renders into.
  // We can make the audio unit provide a buffer instead in io_data, but we
  // currently just use our own.
  // TODO(henrika): should error handling be improved?
  result = audio_input_unit_->Render(flags, time_stamp, bus_number, num_frames, &audio_buffer_list);
  if (result != noErr) {
    RTCLogError(@"Failed to render audio.");
    return result;
  }

  // Get a pointer to the recorded audio and send it to the WebRTC ADB.
  // Use the FineAudioBuffer instance to convert between native buffer size
  // and the 10ms buffer size used by WebRTC.
  record_fine_audio_buffer_->DeliverRecordedData(record_audio_buffer_, kFixedRecordDelayEstimate);
  return noErr;
}

OSStatus AudioDeviceIOS::OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                          const AudioTimeStamp* time_stamp,
                                          UInt32 bus_number,
                                          UInt32 num_frames,
                                          AudioBufferList* io_data) {
  RTC_DCHECK_RUN_ON(&io_playout_thread_checker_);
  // Verify 16-bit, noninterleaved mono PCM signal format.
  RTC_DCHECK_EQ(1, io_data->mNumberBuffers);
  AudioBuffer* audio_buffer = &io_data->mBuffers[0];
  RTC_DCHECK_EQ(1, audio_buffer->mNumberChannels);

  // Produce silence and give audio unit a hint about it if playout is not
  // activated.
  if (!rtc::AtomicOps::AcquireLoad(&playing_)) {
    const size_t size_in_bytes = audio_buffer->mDataByteSize;
    RTC_CHECK_EQ(size_in_bytes / AudioOutputUnit::kBytesPerSample, num_frames);
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(static_cast<int8_t*>(audio_buffer->mData), 0, size_in_bytes);
    return noErr;
  }

  // Measure time since last call to OnGetPlayoutData() and see if it is larger
  // than a well defined threshold which depends on the current IO buffer size.
  // If so, we have an indication of a glitch in the output audio since the
  // core audio layer will most likely run dry in this state.
  ++num_playout_callbacks_;
  const int64_t now_time = rtc::TimeMillis();
  if (time_stamp->mSampleTime != num_frames) {
    const int64_t delta_time = now_time - last_playout_time_;
    const int glitch_threshold = 1.6 * playout_parameters_.GetBufferSizeInMilliseconds();
    if (delta_time > glitch_threshold) {
      RTCLogWarning(@"Possible playout audio glitch detected.\n"
                     "  Time since last OnGetPlayoutData was %lld ms.\n",
                    delta_time);
      // Exclude extreme delta values since they do most likely not correspond
      // to a real glitch. Instead, the most probable cause is that a headset
      // has been plugged in or out. There are more direct ways to detect
      // audio device changes (see HandleValidRouteChange()) but experiments
      // show that using it leads to more complex implementations.
      // TODO(henrika): more tests might be needed to come up with an even
      // better upper limit.
      if (glitch_threshold < 120 && delta_time > 120) {
        RTCLog(@"Glitch warning is ignored. Probably caused by device switch.");
      } else {
        thread_->Post(RTC_FROM_HERE, this, kMessageTypePlayoutGlitchDetected);
      }
    }
  }
  last_playout_time_ = now_time;

  // Read decoded 16-bit PCM samples from WebRTC (using a size that matches
  // the native I/O audio unit) and copy the result to the audio buffer in the
  // `io_data` destination.
  playout_fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int16_t>(static_cast<int16_t*>(audio_buffer->mData), num_frames),
      kFixedPlayoutDelayEstimate);
  return noErr;
}

void AudioDeviceIOS::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case kMessageTypeInterruptionBegin:
      HandleInterruptionBegin();
      break;
    case kMessageTypeInterruptionEnd:
      HandleInterruptionEnd();
      break;
    case kMessageTypeValidRouteChange:
      HandleValidRouteChange();
      break;
    case kMessageTypeCanPlayOrRecordChange: {
      rtc::TypedMessageData<bool>* data = static_cast<rtc::TypedMessageData<bool>*>(msg->pdata);
      HandleCanPlayOrRecordChange(data->data());
      delete data;
      break;
    }
    case kMessageTypePlayoutGlitchDetected:
      HandlePlayoutGlitchDetected();
      break;
    case kMessageOutputVolumeChange:
      HandleOutputVolumeChange();
      break;
  }
}

void AudioDeviceIOS::HandleInterruptionBegin() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Interruption begin. IsInterrupted changed from %d to 1.", is_interrupted_);

  if (auto audio_unit = audio_input_unit_.get()) {
    if (audio_unit->GetState() == BaseAudioUnit::State::kStarted) {
      RTCLog(@"Stopping the audio input unit due to interruption begin.");
      if (!audio_unit->Stop()) {
        RTCLogError(@"Failed to stop the audio input unit for interruption begin.");
      }
      PrepareForNewStart(TargetAudioUnit::kInput);
    }
  }
  if (auto audio_unit = audio_output_unit_.get()) {
    if (audio_unit->GetState() == BaseAudioUnit::State::kStarted) {
      RTCLog(@"Stopping the audio output unit due to interruption begin.");
      if (!audio_unit->Stop()) {
        RTCLogError(@"Failed to stop the audio ouput unit for interruption begin.");
      }
      PrepareForNewStart(TargetAudioUnit::kOutput);
    }
  }

  is_interrupted_ = true;
}

void AudioDeviceIOS::HandleInterruptionEnd() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Interruption ended. IsInterrupted changed from %d to 0. "
          "Updating audio unit state.",
         is_interrupted_);
  is_interrupted_ = false;

  const auto restart_audio_units = webrtc::field_trial::IsEnabled("WebRTC-Audio-iOS-Holding");
  if (restart_audio_units) {
    if (auto audio_unit = audio_input_unit_.get()) {
      // Work around an issue where audio does not restart properly after an interruption
      // by restarting the audio unit when the interruption ends.
      if (audio_unit->GetState() == BaseAudioUnit::State::kStarted) {
        audio_unit->Stop();
        PrepareForNewStart(TargetAudioUnit::kInput);
      }
      if (audio_unit->GetState() == BaseAudioUnit::State::kInitialized) {
        audio_unit->Uninitialize();
      }
      // Allocate new buffers given the potentially new stream format.
      SetupAudioBuffersForActiveAudioSession(TargetAudioUnit::kInput);
    }
    if (auto audio_unit = audio_output_unit_.get()) {
      // Work around an issue where audio does not restart properly after an interruption
      // by restarting the audio unit when the interruption ends.
      if (audio_unit->GetState() == BaseAudioUnit::State::kStarted) {
        audio_unit->Stop();
        PrepareForNewStart(TargetAudioUnit::kOutput);
      }
      if (audio_unit->GetState() == BaseAudioUnit::State::kInitialized) {
        audio_unit->Uninitialize();
      }
      // Allocate new buffers given the potentially new stream format.
      SetupAudioBuffersForActiveAudioSession(TargetAudioUnit::kOutput);
    }
  }
  const auto can_play_or_record = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance].canPlayOrRecord;
  if (audio_input_unit_) {
    UpdateAudioUnit(TargetAudioUnit::kInput, can_play_or_record);
  }
  if (audio_output_unit_) {
    UpdateAudioUnit(TargetAudioUnit::kOutput, can_play_or_record);
  }
}

void AudioDeviceIOS::HandleValidRouteChange() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  RTCLog(@"%@", session);
  HandleSampleRateChange(session.sampleRate);
}

void AudioDeviceIOS::HandleCanPlayOrRecordChange(bool can_play_or_record) {
  RTCLog(@"Handling CanPlayOrRecord change to: %d", can_play_or_record);
  if (audio_input_unit_) {
    UpdateAudioUnit(TargetAudioUnit::kInput, recording_);
  }
  if (audio_output_unit_) {
    UpdateAudioUnit(TargetAudioUnit::kOutput, playing_);
  }
}

void AudioDeviceIOS::HandleSampleRateChange(float sample_rate) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Handling sample rate change to %f.", sample_rate);

  // Don't do anything if we're interrupted.
  if (is_interrupted_) {
    RTCLog(@"Ignoring sample rate change to %f due to interruption.", sample_rate);
    return;
  }

  // If we don't have an audio units yet, or the audio units are not uninitialized,
  // there is no work to do.
  if ((!audio_input_unit_ || audio_input_unit_->GetState() < BaseAudioUnit::State::kInitialized) &&
      (!audio_output_unit_ ||
       audio_output_unit_->GetState() < BaseAudioUnit::State::kInitialized)) {
    return;
  }

  // The audio unit is already initialized or started.
  // Check to see if the sample rate or buffer size has changed.
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  const double session_sample_rate = session.sampleRate;
  const NSTimeInterval session_buffer_duration = session.IOBufferDuration;
  const size_t session_frames_per_buffer =
      static_cast<size_t>(session_sample_rate * session_buffer_duration + .5);
  const double current_sample_rate = playout_parameters_.sample_rate();
  const size_t current_frames_per_buffer = playout_parameters_.frames_per_buffer();
  RTCLog(@"Handling playout sample rate change to: %f\n"
          "  Session sample rate: %f frames_per_buffer: %lu\n"
          "  ADM sample rate: %f frames_per_buffer: %lu",
         sample_rate,
         session_sample_rate,
         (unsigned long)session_frames_per_buffer,
         current_sample_rate,
         (unsigned long)current_frames_per_buffer);

  // Sample rate and buffer size are the same, no work to do.
  if (std::abs(current_sample_rate - session_sample_rate) <= DBL_EPSILON &&
      current_frames_per_buffer == session_frames_per_buffer) {
    RTCLog(@"Ignoring sample rate change since audio parameters are intact.");
    return;
  }

  // Extra sanity check to ensure that the new sample rate is valid.
  if (session_sample_rate <= 0.0) {
    RTCLogError(@"Sample rate is invalid: %f", session_sample_rate);
    return;
  }

  // We need to adjust our format and buffer sizes.
  // The stream format is about to be changed and it requires that we first
  // stop and uninitialize the audio unit to deallocate its resources.
  RTCLog(@"Stopping and uninitializing audio unit to adjust buffers.");
  if (audio_input_unit_) {
    bool restart_audio_unit = false;
    if (audio_input_unit_->GetState() == BaseAudioUnit::State::kStarted) {
      audio_input_unit_->Stop();
      PrepareForNewStart(TargetAudioUnit::kInput);
      restart_audio_unit = true;
    }
    if (audio_input_unit_->GetState() == BaseAudioUnit::State::kInitialized) {
      audio_input_unit_->Uninitialize();
    }
    // Allocate new buffers given the new stream format.
    SetupAudioBuffersForActiveAudioSession(TargetAudioUnit::kInput);

    RTC_DCHECK_EQ(record_parameters_.sample_rate(), session_sample_rate);
    if (!audio_input_unit_->Initialize(session_sample_rate)) {
      RTCLogError(@"Failed to initialize the audio input unit with sample rate: %f",
                  session_sample_rate);
      return;
    }

    // Restart the audio input unit if it was already running.
    if (restart_audio_unit) {
      OSStatus result = audio_input_unit_->Start();
      if (result != noErr) {
        [session notifyAudioInputUnitStartFailedWithError:result];
        RTCLogError(@"Failed to start audio input unit with sample rate: %f, reason %d",
                    session_sample_rate,
                    result);
        return;
      }
    }
  }

  if (audio_output_unit_) {
    bool restart_audio_unit = false;
    if (audio_output_unit_->GetState() == BaseAudioUnit::State::kStarted) {
      audio_output_unit_->Stop();
      PrepareForNewStart(TargetAudioUnit::kOutput);
      restart_audio_unit = true;
    }
    if (audio_output_unit_->GetState() == BaseAudioUnit::State::kInitialized) {
      audio_output_unit_->Uninitialize();
    }
    // Allocate new buffers given the new stream format.
    SetupAudioBuffersForActiveAudioSession(TargetAudioUnit::kOutput);

    RTC_DCHECK_EQ(playout_parameters_.sample_rate(), session_sample_rate);
    if (!audio_output_unit_->Initialize(session_sample_rate)) {
      RTCLogError(@"Failed to initialize the audio output unit with sample rate: %f",
                  session_sample_rate);
      return;
    }

    // Restart the audio output unit if it was already running.
    if (restart_audio_unit) {
      OSStatus result = audio_output_unit_->Start();
      if (result != noErr) {
        [session notifyAudioOutputUnitStartFailedWithError:result];
        RTCLogError(@"Failed to start audio output unit with sample rate: %f, reason %d",
                    session_sample_rate,
                    result);
        return;
      }
    }
  }

  RTCLog(@"Successfully handled sample rate change.");
}

void AudioDeviceIOS::HandlePlayoutGlitchDetected() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  // Don't update metrics if we're interrupted since a "glitch" is expected
  // in this state.
  if (is_interrupted_) {
    RTCLog(@"Ignoring audio glitch due to interruption.");
    return;
  }
  // Avoid doing glitch detection for two seconds after a volume change
  // has been detected to reduce the risk of false alarm.
  if (last_output_volume_change_time_ > 0 &&
      rtc::TimeSince(last_output_volume_change_time_) < 2000) {
    RTCLog(@"Ignoring audio glitch due to recent output volume change.");
    return;
  }
  num_detected_playout_glitches_++;
  RTCLog(@"Number of detected playout glitches: %lld", num_detected_playout_glitches_);

  int64_t glitch_count = num_detected_playout_glitches_;
  dispatch_async(dispatch_get_main_queue(), ^{
    RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    [session notifyDidDetectPlayoutGlitch:glitch_count];
  });
}

void AudioDeviceIOS::HandleOutputVolumeChange() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Output volume change detected.");
  // Store time of this detection so it can be used to defer detection of
  // glitches too close in time to this event.
  last_output_volume_change_time_ = rtc::TimeMillis();
}

void AudioDeviceIOS::UpdateAudioDeviceBuffer(TargetAudioUnit target) {
  LOGI() << "UpdateAudioDevicebuffer target = " << target;
  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";

  if (target == TargetAudioUnit::kInput) {
    RTC_DCHECK_EQ(record_parameters_.channels(), 1);
    RTC_DCHECK_GT(record_parameters_.sample_rate(), 0);
    // Inform the audio device buffer (ADB) about the new audio format.
    audio_device_buffer_->SetRecordingSampleRate(record_parameters_.sample_rate());
    audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());
  } else {
    RTC_DCHECK_EQ(playout_parameters_.channels(), 1);
    RTC_DCHECK_GT(playout_parameters_.sample_rate(), 0);
    // Inform the audio device buffer (ADB) about the new audio format.
    audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
    audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  }
}

void AudioDeviceIOS::SetupAudioBuffersForActiveAudioSession(TargetAudioUnit target) {
  LOGI() << "SetupAudioBuffersForActiveAudioSession target = " << target;
  // Verify the current values once the audio session has been activated.
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  double sample_rate = session.sampleRate;
  NSTimeInterval io_buffer_duration = session.IOBufferDuration;
  RTCLog(@"%@", session);

  // Log a warning message for the case when we are unable to set the preferred
  // hardware sample rate but continue and use the non-ideal sample rate after
  // reinitializing the audio parameters. Most BT headsets only support 8kHz or
  // 16kHz.
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* webRTCConfig =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  if (sample_rate != webRTCConfig.sampleRate) {
    RTC_LOG(LS_WARNING) << "Actual sample rate " << sample_rate
                        << " differs from requested sample rate " << webRTCConfig.sampleRate;
  }

  AudioParameters& target_parameters =
      target == TargetAudioUnit::kInput ? record_parameters_ : playout_parameters_;

  // Crash reports indicates that it can happen in rare cases that the reported
  // sample rate is less than or equal to zero. If that happens and if a valid
  // sample rate has already been set during initialization, the best guess we
  // can do is to reuse the current sample rate.
  if (sample_rate <= DBL_EPSILON && target_parameters.sample_rate() > 0) {
    RTCLogError(@"Reported rate is invalid: %f. "
                 "Using %d as sample rate instead.",
                sample_rate,
                target_parameters.sample_rate());
    sample_rate = target_parameters.sample_rate();
  }

  // At this stage, we also know the exact IO buffer duration and can add
  // that info to the existing audio parameters where it is converted into
  // number of audio frames.
  // Example: IO buffer size = 0.008 seconds <=> 128 audio frames at 16kHz.
  // Hence, 128 is the size we expect to see in upcoming render callbacks.
  target_parameters.reset(sample_rate, target_parameters.channels(), io_buffer_duration);
  RTC_DCHECK(target_parameters.is_complete());

  RTC_LOG(LS_INFO) << "Audio parameters: " << target_parameters.ToString();

  // Update the ADB parameters since the sample rate might have changed.
  UpdateAudioDeviceBuffer(target);

  // Create a modified audio buffer class which allows us to ask for,
  // or deliver, any number of samples (and not only multiple of 10ms) to match
  // the native audio unit buffer size.
  RTC_DCHECK(audio_device_buffer_);
  if (target == TargetAudioUnit::kInput) {
    record_fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_));
  } else {
    playout_fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_));
  }
}

bool AudioDeviceIOS::CreateAudioInputUnit() {
  RTC_DCHECK(!audio_input_unit_);

  const auto input_unit_type = bypass_voice_processing_ ?
      AudioInputUnit::UnitType::kRemoteIO :
      AudioInputUnit::UnitType::kVoiceProcessingIO;
  audio_input_unit_.reset(new AudioInputUnit(input_unit_type, this));
  if (!audio_input_unit_->Init()) {
    RTCLogError(@"Unable to init audio input unit");
    audio_input_unit_.reset();
    return false;
  }

  return true;
}

bool AudioDeviceIOS::CreateAudioOutputUnit() {
  RTC_DCHECK(!audio_output_unit_);

  // Use VPIO unit when voice processing is enabled, this is necesary
  // to avoid ducking RemoteIO unit by VPIO unit.
  const auto output_unit_type = bypass_voice_processing_ ?
      AudioOutputUnit::UnitType::kRemoteIO :
      AudioOutputUnit::UnitType::kVoiceProcessingIO;

  audio_output_unit_.reset(new AudioOutputUnit(output_unit_type, this));
  if (!audio_output_unit_->Init()) {
    RTCLogError(@"Unable to init audio output unit");
    audio_output_unit_.reset();
    return false;
  }

  return true;
}

void AudioDeviceIOS::UpdateAudioUnit(TargetAudioUnit target, const bool can_play_or_record) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Updating audio unit state. CanPlayOrRecord=%d IsInterrupted=%d Target=%d",
         can_play_or_record,
         is_interrupted_,
         target);

  if (is_interrupted_) {
    RTCLog(@"Ignoring audio unit update due to interruption.");
    return;
  }

  BaseAudioUnit* audio_unit;
  bool should_play_or_record;
  AudioParameters* audio_params;
  if (target == TargetAudioUnit::kInput) {
    audio_unit = audio_input_unit_.get();
    should_play_or_record = recording_;
    audio_params = &record_parameters_;
  } else {
    audio_unit = audio_output_unit_.get();
    should_play_or_record = playing_;
    audio_params = &playout_parameters_;
  }
  RTC_DCHECK(audio_unit);

  bool should_initialize_audio_unit = false;
  bool should_uninitialize_audio_unit = false;
  bool should_start_audio_unit = false;
  bool should_stop_audio_unit = false;

  switch (audio_unit->GetState()) {
    case BaseAudioUnit::State::kInitRequired:
      RTCLog(@"BaseAudioUnit state: InitRequired");
      RTC_DCHECK_NOTREACHED();
      break;
    case BaseAudioUnit::State::kUninitialized:
      RTCLog(@"BaseAudioUnit state: Uninitialized");
      should_initialize_audio_unit = can_play_or_record;
      should_start_audio_unit = should_initialize_audio_unit && should_play_or_record;
      break;
    case BaseAudioUnit::State::kInitialized:
      RTCLog(@"BaseAudioUnit state: Initialized");
      should_start_audio_unit = can_play_or_record && should_play_or_record;
      should_uninitialize_audio_unit = !can_play_or_record;
      break;
    case BaseAudioUnit::State::kStarted:
      RTCLog(@"BaseAudioUnit state: Started");
      RTC_DCHECK(should_play_or_record);
      should_stop_audio_unit = !can_play_or_record;
      should_uninitialize_audio_unit = should_stop_audio_unit;
      break;
  }

  if (should_initialize_audio_unit) {
    RTCLog(@"Initializing audio unit for UpdateAudioUnit");
    ConfigureAudioSession();
    SetupAudioBuffersForActiveAudioSession(target);

    if (!audio_unit->Initialize(audio_params->sample_rate())) {
      RTCLogError(@"Failed to initialize audio unit.");
      return;
    }
  }

  if (should_start_audio_unit) {
    RTCLog(@"Starting audio unit for UpdateAudioUnit");
    // Log session settings before trying to start audio streaming.
    RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    RTCLog(@"%@", session);
    OSStatus result = audio_unit->Start();
    if (result != noErr) {
      if (target == TargetAudioUnit::kInput) {
        [session notifyAudioInputUnitStartFailedWithError:result];
      } else {
        [session notifyAudioOutputUnitStartFailedWithError:result];
      }
      RTCLogError(@"Failed to start audio unit, reason %d", result);
      return;
    }
  }

  if (should_stop_audio_unit) {
    RTCLog(@"Stopping audio unit for UpdateAudioUnit");
    if (!audio_unit->Stop()) {
      RTCLogError(@"Failed to stop audio unit.");
      PrepareForNewStart(target);
      return;
    }
    PrepareForNewStart(target);
  }

  if (should_uninitialize_audio_unit) {
    RTCLog(@"Uninitializing audio unit for UpdateAudioUnit");
    audio_unit->Uninitialize();
    UnconfigureAudioSession();
  }
}

bool AudioDeviceIOS::ConfigureAudioSession() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Configuring audio session.");
  if (has_configured_session_) {
    RTCLogWarning(@"Audio session already configured.");
    return false;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  bool success = [session configureWebRTCSession:nil];
  [session unlockForConfiguration];
  if (success) {
    has_configured_session_ = true;
    RTCLog(@"Configured audio session.");
  } else {
    RTCLog(@"Failed to configure audio session.");
  }
  return success;
}

bool AudioDeviceIOS::ConfigureAudioSessionLocked() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Configuring audio session.");
  if (has_configured_session_) {
    RTCLogWarning(@"Audio session already configured.");
    return false;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  bool success = [session configureWebRTCSession:nil];
  if (success) {
    has_configured_session_ = true;
    RTCLog(@"Configured audio session.");
  } else {
    RTCLog(@"Failed to configure audio session.");
  }
  return success;
}

void AudioDeviceIOS::UnconfigureAudioSession() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTCLog(@"Unconfiguring audio session.");
  if (!has_configured_session_) {
    RTCLogWarning(@"Audio session already unconfigured.");
    return;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  [session unconfigureWebRTCSession:nil];
  [session endWebRTCSession:nil];
  [session unlockForConfiguration];
  has_configured_session_ = false;
  RTCLog(@"Unconfigured audio session.");
}

bool AudioDeviceIOS::InitializeAudioSession(bool& can_play_or_record) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  LOGI() << "InitializeAudioSession";

  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  // Subscribe to audio session events.
  if (!is_audio_session_subscribed_) {
    [session pushDelegate:audio_session_observer_];
    is_audio_session_subscribed_ = true;
  }
  is_interrupted_ = session.isInterrupted ? true : false;

  // Lock the session to make configuration changes.
  [session lockForConfiguration];
  NSError* error = nil;
  if (![session beginWebRTCSession:&error]) {
    [session unlockForConfiguration];
    RTCLogError(@"Failed to begin WebRTC session: %@", error.localizedDescription);
    return false;
  }

  // If we are ready to play or record, and if the audio session can be
  // configured, then initialize the audio unit.
  if (session.canPlayOrRecord) {
    if (!ConfigureAudioSessionLocked()) {
      // One possible reason for failure is if an attempt was made to use the
      // audio session during or after a Media Services failure.
      // See AVAudioSessionErrorCodeMediaServicesFailed for details.
      [session unlockForConfiguration];
      return false;
    }
    can_play_or_record = true;
  } else {
    can_play_or_record = false;
  }

  // Release the lock.
  [session unlockForConfiguration];
  return true;
}

void AudioDeviceIOS::Shutdown() {
  LOGI() << "Shutdown";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(audio_input_unit_ == nullptr);
  RTC_DCHECK(audio_output_unit_ == nullptr);

  // Remove audio session notification observers.
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  if (is_audio_session_subscribed_) {
    [session removeDelegate:audio_session_observer_];
    is_audio_session_subscribed_ = false;
  }

  // All I/O should be stopped or paused prior to deactivating the audio
  // session, hence we deactivate as last action.
  UnconfigureAudioSession();
}

void AudioDeviceIOS::PrepareForNewStart(TargetAudioUnit target) {
  LOGI() << "PrepareForNewStart target = " << target;
  // The audio unit has been stopped and preparations are needed for an upcoming
  // restart. It will result in audio callbacks from a new native I/O thread
  // which means that we must detach thread checkers here to be prepared for an
  // upcoming new audio stream.
  if (target == TargetAudioUnit::kInput) {
    io_record_thread_checker_.Detach();
  } else {
    io_playout_thread_checker_.Detach();
  }
}

bool AudioDeviceIOS::IsInterrupted() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return is_interrupted_;
}

#pragma mark - Not Implemented

int32_t AudioDeviceIOS::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int16_t AudioDeviceIOS::PlayoutDevices() {
  // TODO(henrika): improve.
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int16_t AudioDeviceIOS::RecordingDevices() {
  // TODO(henrika): improve.
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int32_t AudioDeviceIOS::InitSpeaker() {
  return 0;
}

bool AudioDeviceIOS::SpeakerIsInitialized() const {
  return true;
}

int32_t AudioDeviceIOS::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetSpeakerVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetSpeakerMute(bool enable) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerMute(bool& enabled) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::InitMicrophone() {
  return 0;
}

bool AudioDeviceIOS::MicrophoneIsInitialized() const {
  return true;
}

int32_t AudioDeviceIOS::MicrophoneMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneMute(bool enable) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneMute(bool& enabled) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoRecording(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoRecording(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoPlayout(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoPlayout(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::PlayoutDeviceName(uint16_t index,
                                          char name[kAdmMaxDeviceNameSize],
                                          char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::RecordingDeviceName(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetRecordingDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceIOS::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

}  // namespace ios_adm
}  // namespace webrtc
