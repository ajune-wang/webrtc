/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "audio_input_unit.h"

#include "absl/base/macros.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"
#import "sdk/objc/components/audio/RTCAudioSessionConfiguration.h"

namespace webrtc {
namespace ios_adm {

// Returns the automatic gain control (AGC) state on the processed microphone
// signal. Should be on by default for Voice Processing audio units.
static OSStatus GetAGCState(AudioUnit audio_unit, UInt32* enabled) {
  RTC_DCHECK(audio_unit);
  UInt32 size = sizeof(*enabled);
  OSStatus result = AudioUnitGetProperty(audio_unit,
                                         kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                                         kAudioUnitScope_Global,
                                         BaseAudioUnit::kInputBus,
                                         enabled,
                                         &size);
  RTCLog(@"VPIO unit AGC: %u", static_cast<unsigned int>(*enabled));
  return result;
}

AudioInputUnit::AudioInputUnit(UnitType unit_type, AudioInputUnitObserver* observer)
    : audio_unit_type_(unit_type), observer_(observer) {
  RTC_DCHECK(observer);
}

BaseAudioUnit::OwnedAudioUnit AudioInputUnit::InstantiateAudioUnit() {
  RTCLog(@"Will instantiate %@ audio unit",
         audio_unit_type_ == UnitType::kVoiceProcessingIO ? @"VPIO" : @"RemoteIO");
  // Create an audio component description to identify an audio unit.
  AudioComponentDescription audio_unit_description = {
      .componentType = kAudioUnitType_Output,
      .componentSubType = audio_unit_type_ == UnitType::kVoiceProcessingIO ?
          kAudioUnitSubType_VoiceProcessingIO :
          kAudioUnitSubType_RemoteIO,
      .componentManufacturer = kAudioUnitManufacturer_Apple,
      .componentFlags = 0,
      .componentFlagsMask = 0,
  };

  // Obtain an audio unit instance given the description.
  AudioComponent found_audio_unit_ref = AudioComponentFindNext(nullptr, &audio_unit_description);

  AudioUnit unit;
  // Create an audio unit.
  OSStatus result = noErr;
  result = AudioComponentInstanceNew(found_audio_unit_ref, &unit);
  if (result != noErr) {
    RTCLogError(@"AudioComponentInstanceNew failed. Error=%ld.", (long)result);
    return nullptr;
  }

  BaseAudioUnit::OwnedAudioUnit audio_unit{unit};

  // Enable input on the input scope of the input element.
  UInt32 enable_input = 1;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                kInputBus,
                                &enable_input,
                                sizeof(enable_input));
  if (result != noErr) {
    RTCLogError(@"Failed to enable input on input scope of input element. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }

  // Disable output on the output scope of the output element.
  UInt32 disable_output = 0;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                kOutputBus,
                                &disable_output,
                                sizeof(disable_output));
  if (result != noErr) {
    RTCLogError(@"Failed to disable output on output scope of output element. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }

  // Disable AU buffer allocation for the recorder, we allocate our own.
  // TODO(henrika): not sure that it actually saves resource to make this call.
  UInt32 flag = 0;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioUnitProperty_ShouldAllocateBuffer,
                                kAudioUnitScope_Output,
                                kInputBus,
                                &flag,
                                sizeof(flag));
  if (result != noErr) {
    RTCLogError(@"Failed to disable buffer allocation on the input bus. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }

  // Specify the callback to be called by the I/O thread to us when input audio
  // is available. The recorded samples can then be obtained by calling the
  // AudioUnitRender() method.
  AURenderCallbackStruct input_callback;
  input_callback.inputProc = OnDeliverRecordedData;
  input_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioOutputUnitProperty_SetInputCallback,
                                kAudioUnitScope_Global,
                                kInputBus,
                                &input_callback,
                                sizeof(input_callback));
  if (result != noErr) {
    RTCLogError(@"Failed to specify the input callback on the input bus. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }

  return audio_unit;
}

void AudioInputUnit::UpdatePropertiesPostInitialize() {
  AudioUnit audio_unit = this->audio_unit();
  RTC_DCHECK(audio_unit != nullptr);

  OSStatus result = noErr;
  if (audio_unit_type_ != UnitType::kVoiceProcessingIO) {
    return;
  }
  // AGC should be enabled by default for Voice Processing I/O units but it is
  // checked below and enabled explicitly if needed. This scheme is used
  // to be absolutely sure that the AGC is enabled since we have seen cases
  // where only zeros are recorded and a disabled AGC could be one of the
  // reasons why it happens.
  int agc_was_enabled_by_default = 0;
  UInt32 agc_is_enabled = 0;
  result = GetAGCState(audio_unit, &agc_is_enabled);
  if (result != noErr) {
    RTCLogError(@"Failed to get AGC state (1st attempt). "
                 "Error=%ld.",
                (long)result);
    // Example of error code: kAudioUnitErr_NoConnection (-10876).
    // All error codes related to audio units are negative and are therefore
    // converted into a postive value to match the UMA APIs.
    RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Audio.GetAGCStateErrorCode1", (-1) * result);
  } else if (agc_is_enabled) {
    // Remember that the AGC was enabled by default. Will be used in UMA.
    agc_was_enabled_by_default = 1;
  } else {
    // AGC was initially disabled => try to enable it explicitly.
    UInt32 enable_agc = 1;
    result = AudioUnitSetProperty(audio_unit,
                                  kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                                  kAudioUnitScope_Global,
                                  kInputBus,
                                  &enable_agc,
                                  sizeof(enable_agc));
    if (result != noErr) {
      RTCLogError(@"Failed to enable the built-in AGC. "
                   "Error=%ld.",
                  (long)result);
      RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Audio.SetAGCStateErrorCode", (-1) * result);
    }
    result = GetAGCState(audio_unit, &agc_is_enabled);
    if (result != noErr) {
      RTCLogError(@"Failed to get AGC state (2nd attempt). "
                   "Error=%ld.",
                  (long)result);
      RTC_HISTOGRAM_COUNTS_SPARSE_100000("WebRTC.Audio.GetAGCStateErrorCode2", (-1) * result);
    }
  }

  // Track if the built-in AGC was enabled by default (as it should) or not.
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.BuiltInAGCWasEnabledByDefault", agc_was_enabled_by_default);
  RTCLog(@"WebRTC.Audio.BuiltInAGCWasEnabledByDefault: %d", agc_was_enabled_by_default);
  // As a final step, add an UMA histogram for tracking the AGC state.
  // At this stage, the AGC should be enabled, and if it is not, more work is
  // needed to find out the root cause.
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.BuiltInAGCIsEnabled", agc_is_enabled);
  RTCLog(@"WebRTC.Audio.BuiltInAGCIsEnabled: %u", static_cast<unsigned int>(agc_is_enabled));
}

OSStatus AudioInputUnit::Render(AudioUnitRenderActionFlags* flags,
                                const AudioTimeStamp* time_stamp,
                                UInt32 output_bus_number,
                                UInt32 num_frames,
                                AudioBufferList* io_data) {
  AudioUnit audio_unit = this->audio_unit();
  RTC_DCHECK(audio_unit) << "Init() not called.";

  OSStatus result =
      AudioUnitRender(audio_unit, flags, time_stamp, output_bus_number, num_frames, io_data);
  if (result != noErr) {
    RTCLogError(@"Failed to render audio unit. Error=%ld", (long)result);
  }
  return result;
}

OSStatus AudioInputUnit::OnDeliverRecordedData(void* in_ref_con,
                                               AudioUnitRenderActionFlags* flags,
                                               const AudioTimeStamp* time_stamp,
                                               UInt32 bus_number,
                                               UInt32 num_frames,
                                               AudioBufferList* io_data) {
  AudioInputUnit* audio_unit = static_cast<AudioInputUnit*>(in_ref_con);
  return audio_unit->NotifyDeliverRecordedData(flags, time_stamp, bus_number, num_frames, io_data);
}

OSStatus AudioInputUnit::NotifyDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                                   const AudioTimeStamp* time_stamp,
                                                   UInt32 bus_number,
                                                   UInt32 num_frames,
                                                   AudioBufferList* io_data) {
  return observer_->OnDeliverRecordedData(flags, time_stamp, bus_number, num_frames, io_data);
}

}  // namespace ios_adm
}  // namespace webrtc
