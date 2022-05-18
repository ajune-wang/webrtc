/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include "objc_audio_device.h"

#include <cmath>

#include "api/array_view.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#import "base/RTCLogging.h"

#import "components/audio/RTCAudioSessionConfiguration.h"

namespace webrtc {
namespace objc_adm {

const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

ObjCAudioDevice::ObjCAudioDevice(id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device)
    : audio_device_buffer_(nullptr), audio_device_(audio_device), audio_device_delegate_(nil) {
  RTC_LOG_F(LS_INFO) << [[[NSThread currentThread] description] UTF8String];
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();
  thread_checker_.Detach();
  thread_ = rtc::Thread::Current();
}

ObjCAudioDevice::~ObjCAudioDevice() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG_F(LS_INFO) << [[[NSThread currentThread] description] UTF8String];
  Terminate();
}

void ObjCAudioDevice::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK(thread_checker_.IsCurrent());
  audio_device_buffer_ = audioBuffer;
}

bool ObjCAudioDevice::Initialized() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return is_initialized_ && [audio_device_ isInitialized];
}

AudioDeviceGeneric::InitStatus ObjCAudioDevice::Init() {
  RTC_LOG_F(LS_INFO) << "";
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();
  thread_checker_.Detach();

  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (Initialized()) {
    RTC_LOG_F(LS_INFO) << "Already initialized";
    return InitStatus::OK;
  }

  if (![audio_device_ isInitialized]) {
    if (audio_device_delegate_ == nil) {
      audio_device_delegate_ = [[ObjCAudioDeviceDelegate alloc] initWithUnownedAudioDevice:this
                                                                         audioDeviceThread:thread_];
    }

    if (![audio_device_ initializeWithDelegate:audio_device_delegate_]) {
      RTC_LOG_F(LS_INFO) << "Failed to initialize";
      [audio_device_delegate_ resetAudioDevice];
      audio_device_delegate_ = nil;
      return InitStatus::OTHER_ERROR;
    }
  }

  playout_parameters_.reset([audio_device_ outputSampleRate], 1);
  record_parameters_.reset([audio_device_ inputSampleRate], 1);

  UpdateAudioDeviceBuffer();

  is_initialized_ = true;

  RTC_LOG_F(LS_INFO) << "Did initialize";
  return InitStatus::OK;
}

int32_t ObjCAudioDevice::Terminate() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (!Initialized()) {
    RTC_LOG_F(LS_INFO) << "Not initialized";
    return 0;
  }

  if (![audio_device_ terminate]) {
    RTC_LOG_F(LS_ERROR) << "Failed to terminate";
    return -1;
  }
  if (audio_device_delegate_ != nil) {
    // Buggy implementation of RTCAudioDevice may still store pointer to RTCAudioDeviceDelegate,
    // so manually reset RTCAudioDeviceDelegate's pointer to ObjCAudioDevice.
    [audio_device_delegate_ resetAudioDevice];
    audio_device_delegate_ = nil;
  }

  is_initialized_ = false;
  is_playout_initialized_ = false;
  is_recording_initialized_ = false;

  RTC_LOG_F(LS_INFO) << "Did terminate";
  return 0;
}

bool ObjCAudioDevice::PlayoutIsInitialized() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return is_playout_initialized_ && [audio_device_ isPlayoutInitialized];
}

bool ObjCAudioDevice::Playing() const {
  return playing_ && [audio_device_ isPlaying];
}

int32_t ObjCAudioDevice::InitPlayout() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(Initialized());
  RTC_DCHECK(!PlayoutIsInitialized());
  RTC_DCHECK(!Playing());

  if (![audio_device_ isPlayoutInitialized]) {
    if (![audio_device_ initializePlayout]) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize playout";
      return -1;
    }
  }

  SetupAudioBuffers(playout_parameters_,
                    [audio_device_ outputSampleRate],
                    [audio_device_ outputIOBufferDuration]);

  is_playout_initialized_ = true;

  RTC_LOG_F(LS_INFO) << "Did initialize playout";
  return 0;
}

int32_t ObjCAudioDevice::StartPlayout() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(PlayoutIsInitialized());
  RTC_DCHECK(!Playing());

  if (playout_fine_audio_buffer_) {
    playout_fine_audio_buffer_->ResetPlayout();
  }
  if (![audio_device_ startPlayout]) {
    RTC_LOG_F(LS_ERROR) << "Failed to start playout";
    return -1;
  }
  playing_ = 1;
  RTC_LOG(LS_INFO) << "Did start playout";
  return 0;
}

int32_t ObjCAudioDevice::StopPlayout() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (![audio_device_ stopPlayout]) {
    RTC_LOG(LS_WARNING) << "Failed to stop playout";
    return -1;
  }
  playing_ = 0;
  RTC_LOG(LS_INFO) << "Did stop playout";
  return 0;
}

bool ObjCAudioDevice::Recording() const {
  return recording_ && [audio_device_ isRecording];
}

bool ObjCAudioDevice::RecordingIsInitialized() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return is_recording_initialized_ && [audio_device_ isRecordingInitialized];
}

int32_t ObjCAudioDevice::InitRecording() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(Initialized());
  RTC_DCHECK(!RecordingIsInitialized());
  RTC_DCHECK(!Recording());

  if (![audio_device_ isRecordingInitialized]) {
    if (![audio_device_ initializeRecording]) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize recording";
      return -1;
    }
  }

  SetupAudioBuffers(
      record_parameters_, [audio_device_ inputSampleRate], [audio_device_ inputIOBufferDuration]);
  is_recording_initialized_ = true;

  RTC_LOG_F(LS_INFO) << "Did initialize recording";
  return 0;
}

int32_t ObjCAudioDevice::StartRecording() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(RecordingIsInitialized());
  RTC_DCHECK(!Recording());

  if (record_fine_audio_buffer_) {
    record_fine_audio_buffer_->ResetRecord();
  }

  if (![audio_device_ startRecording]) {
    RTC_LOG_F(LS_ERROR) << "Failed to start recording";
    return -1;
  }
  recording_ = 1;
  RTC_LOG(LS_INFO) << "Did start recording";
  return 0;
}

int32_t ObjCAudioDevice::StopRecording() {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (![audio_device_ stopRecording]) {
    RTC_LOG(LS_WARNING) << "Failed to stop recording";
    return -1;
  }
  recording_ = 0;
  RTC_LOG(LS_INFO) << "Did stop recording";
  return 0;
}

int32_t ObjCAudioDevice::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

int32_t ObjCAudioDevice::GetPlayoutUnderrunCount() const {
  return -1;
}

int ObjCAudioDevice::GetPlayoutAudioParameters(AudioParameters* params) const {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.IsCurrent());
  *params = playout_parameters_;
  return 0;
}

int ObjCAudioDevice::GetRecordAudioParameters(AudioParameters* params) const {
  RTC_LOG_F(LS_INFO) << "";
  RTC_DCHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.IsCurrent());
  *params = record_parameters_;
  return 0;
}

OSStatus ObjCAudioDevice::OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                                const AudioTimeStamp* time_stamp,
                                                NSInteger bus_number,
                                                UInt32 num_frames,
                                                const AudioBufferList* io_data,
                                                RTC_OBJC_TYPE(RTCAudioDeviceRenderRecordedDataBlock)
                                                    renderBlock) {
  RTC_DCHECK_RUN_ON(&io_record_thread_checker_);
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!rtc::AtomicOps::AcquireLoad(&recording_)) return result;

  if (io_data != nullptr) {
    // AudioBuffer already fullfilled with audio data
    RTC_DCHECK_EQ(1, io_data->mNumberBuffers);
    const AudioBuffer* audio_buffer = &io_data->mBuffers[0];
    RTC_DCHECK_EQ(1, audio_buffer->mNumberChannels);

    record_fine_audio_buffer_->DeliverRecordedData(
        rtc::ArrayView<const int16_t>(static_cast<int16_t*>(audio_buffer->mData), num_frames),
        kFixedRecordDelayEstimate);
    return noErr;
  }
  RTC_DCHECK(renderBlock != nullptr) << "Either io_data or renderBlock must be provided";

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
  audio_buffer->mDataByteSize =
      record_audio_buffer_.size() * sizeof(decltype(record_audio_buffer_)::value_type);
  audio_buffer->mData = reinterpret_cast<int8_t*>(record_audio_buffer_.data());

  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the `io_data` parameter is a reference
  // to the preallocated audio buffer list that the audio unit renders into.
  // We can make the audio unit provide a buffer instead in io_data, but we
  // currently just use our own.
  result = renderBlock(flags, time_stamp, bus_number, num_frames, &audio_buffer_list);
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

OSStatus ObjCAudioDevice::OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                           const AudioTimeStamp* time_stamp,
                                           NSInteger bus_number,
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
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    return noErr;
  }

  // Read decoded 16-bit PCM samples from WebRTC (using a size that matches
  // the native I/O audio unit) and copy the result to the audio buffer in the
  // `io_data` destination.
  playout_fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int16_t>(static_cast<int16_t*>(audio_buffer->mData), num_frames),
      kFixedPlayoutDelayEstimate);
  return noErr;
}

void ObjCAudioDevice::HandleAudioInterrupted() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  io_playout_thread_checker_.Detach();
  io_record_thread_checker_.Detach();
}

void ObjCAudioDevice::HandleAudioParametersChange() {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  struct {
    AudioParameters& native_params;
    double sample_rate;
    NSTimeInterval io_buffer_duration;
  } next_audio_params[] = {{.native_params = playout_parameters_,
                            .sample_rate = [audio_device_ outputSampleRate],
                            .io_buffer_duration = [audio_device_ outputIOBufferDuration]},
                           {.native_params = record_parameters_,
                            .sample_rate = [audio_device_ inputSampleRate],
                            .io_buffer_duration = [audio_device_ inputIOBufferDuration]}};

  for (auto audio_params : next_audio_params) {
    AudioParameters& native_params = audio_params.native_params;
    const double session_sample_rate = audio_params.sample_rate;
    RTCLog(@"Handling sample rate change to %f.", session_sample_rate);

    const NSTimeInterval session_buffer_duration = audio_params.io_buffer_duration;
    const size_t session_frames_per_buffer =
        static_cast<size_t>(session_sample_rate * session_buffer_duration + .5);
    const double current_sample_rate = native_params.sample_rate();
    const size_t current_frames_per_buffer = native_params.frames_per_buffer();
    RTCLog(@"Handling playout sample rate change."
            "  Session sample rate: %f frames_per_buffer: %lu\n"
            "  ADM sample rate: %f frames_per_buffer: %lu",
           session_sample_rate,
           (unsigned long)session_frames_per_buffer,
           current_sample_rate,
           (unsigned long)current_frames_per_buffer);

    // Sample rate and buffer size are the same, no work to do.
    if (std::abs(current_sample_rate - session_sample_rate) <= DBL_EPSILON &&
        current_frames_per_buffer == session_frames_per_buffer) {
      RTCLog(@"Ignoring sample rate change since audio parameters are intact.");
      continue;
    }

    // Extra sanity check to ensure that the new sample rate is valid.
    if (session_sample_rate <= 0.0) {
      RTCLogError(@"Sample rate is invalid: %f", session_sample_rate);
      continue;
    }

    // Allocate new buffers given the new stream format.
    SetupAudioBuffers(native_params, session_sample_rate, session_buffer_duration);

    // Initialize the audio unit again with the new sample rate.
    RTC_DCHECK_EQ(native_params.sample_rate(), session_sample_rate);
  }

  RTCLog(@"Successfully handled sample rate change.");
}

void ObjCAudioDevice::UpdateAudioDeviceBuffer() {
  RTC_LOG_F(LS_INFO) << "";
  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";
  RTC_DCHECK_GT(playout_parameters_.sample_rate(), 0);
  RTC_DCHECK_GT(record_parameters_.sample_rate(), 0);
  RTC_DCHECK_EQ(playout_parameters_.channels(), 1);
  RTC_DCHECK_EQ(record_parameters_.channels(), 1);
  // Inform the audio device buffer (ADB) about the new audio format.
  audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  audio_device_buffer_->SetRecordingSampleRate(record_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());
}

void ObjCAudioDevice::SetupAudioBuffers(AudioParameters& parameters,
                                        double sample_rate,
                                        NSTimeInterval io_buffer_duration) {
  RTC_LOG_F(LS_INFO) << "sample_rate = " << sample_rate
                     << ", io_buffer_duration = " << io_buffer_duration
                     << ", parameters = " << parameters.ToString();

  // Reuse the current sample rate if invalid sample rate provided.
  if (sample_rate <= DBL_EPSILON && parameters.sample_rate() > 0) {
    RTCLogError(@"Reported rate is invalid: %f. "
                 "Using %d as sample rate instead.",
                sample_rate,
                parameters.sample_rate());
    sample_rate = parameters.sample_rate();
  }

  // At this stage, we also know the exact IO buffer duration and can add
  // that info to the existing audio parameters where it is converted into
  // number of audio frames.
  // Example: IO buffer size = 0.008 seconds <=> 128 audio frames at 16kHz.
  // Hence, 128 is the size we expect to see in upcoming render callbacks.
  parameters.reset(sample_rate, parameters.channels(), io_buffer_duration);
  RTC_DCHECK(parameters.is_complete());
  RTC_LOG(LS_INFO) << parameters.ToString();

  // Update the ADB parameters since the sample rate might have changed.
  UpdateAudioDeviceBuffer();

  // Create a modified audio buffer class which allows us to ask for,
  // or deliver, any number of samples (and not only multiple of 10ms) to match
  // the native audio unit buffer size.
  RTC_DCHECK(audio_device_buffer_);
  if (std::addressof(parameters) == std::addressof(playout_parameters_)) {
    playout_fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_));
  } else {
    record_fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_));
  }
}

#pragma mark - Not Implemented

int32_t ObjCAudioDevice::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int16_t ObjCAudioDevice::PlayoutDevices() {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int16_t ObjCAudioDevice::RecordingDevices() {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int32_t ObjCAudioDevice::InitSpeaker() {
  return 0;
}

bool ObjCAudioDevice::SpeakerIsInitialized() const {
  return true;
}

int32_t ObjCAudioDevice::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t ObjCAudioDevice::SetSpeakerVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::SpeakerVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t ObjCAudioDevice::SetSpeakerMute(bool enable) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::SpeakerMute(bool& enabled) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::SetPlayoutDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t ObjCAudioDevice::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::InitMicrophone() {
  return 0;
}

bool ObjCAudioDevice::MicrophoneIsInitialized() const {
  return true;
}

int32_t ObjCAudioDevice::MicrophoneMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t ObjCAudioDevice::SetMicrophoneMute(bool enable) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::MicrophoneMute(bool& enabled) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t ObjCAudioDevice::SetStereoRecording(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::StereoRecording(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t ObjCAudioDevice::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t ObjCAudioDevice::SetStereoPlayout(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::StereoPlayout(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t ObjCAudioDevice::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t ObjCAudioDevice::SetMicrophoneVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::MicrophoneVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::PlayoutDeviceName(uint16_t index,
                                           char name[kAdmMaxDeviceNameSize],
                                           char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::RecordingDeviceName(uint16_t index,
                                             char name[kAdmMaxDeviceNameSize],
                                             char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::SetRecordingDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t ObjCAudioDevice::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t ObjCAudioDevice::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t ObjCAudioDevice::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

}  // namespace objc_adm
}  // namespace webrtc
