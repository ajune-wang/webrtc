/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "audio_output_unit.h"

#include "absl/base/macros.h"
#include "rtc_base/checks.h"

#import "base/RTCLogging.h"

namespace webrtc {
namespace ios_adm {

AudioOutputUnit::AudioOutputUnit(AudioOutputUnit::UnitType type, AudioOutputUnitObserver* observer)
    : observer_(observer), audio_unit_type_(type) {
  RTC_DCHECK(observer);
}

BaseAudioUnit::OwnedAudioUnit AudioOutputUnit::InstantiateAudioUnit() {
  RTCLog(@"Will instantiate %@ audio unit",
         audio_unit_type_ == UnitType::kVoiceProcessingIO ? @"VPIO" : @"RemoteIO");
  // Create an audio component description to identify audio unit.
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

  AudioUnit unit = nullptr;
  // Create an audio unit.
  OSStatus result = noErr;
  result = AudioComponentInstanceNew(found_audio_unit_ref, &unit);
  if (result != noErr) {
    RTCLogError(@"AudioComponentInstanceNew failed. Error=%ld.", (long)result);
    return nullptr;
  }

  BaseAudioUnit::OwnedAudioUnit audio_unit{unit};
  UInt32 enable_input = 0;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input,
                                kInputBus,
                                &enable_input,
                                sizeof(enable_input));
  if (result != noErr) {
    RTCLogError(@"Failed to disable input on input scope of input element. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }

  // Enable output on the output scope of the output element.
  UInt32 enable_output = 1;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                kOutputBus,
                                &enable_output,
                                sizeof(enable_output));
  if (result != noErr) {
    RTCLogError(@"Failed to enable output on output scope of output element. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }

  // Specify the callback function that provides audio samples to the audio
  // unit.
  AURenderCallbackStruct render_callback;
  render_callback.inputProc = OnGetPlayoutData;
  render_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(audio_unit.get(),
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                kOutputBus,
                                &render_callback,
                                sizeof(render_callback));
  if (result != noErr) {
    RTCLogError(@"Failed to specify the render callback on the output bus. "
                 "Error=%ld.",
                (long)result);
    return nullptr;
  }
  return audio_unit;
}

void AudioOutputUnit::UpdatePropertiesPostInitialize() {}

OSStatus AudioOutputUnit::OnGetPlayoutData(void* in_ref_con,
                                           AudioUnitRenderActionFlags* flags,
                                           const AudioTimeStamp* time_stamp,
                                           UInt32 bus_number,
                                           UInt32 num_frames,
                                           AudioBufferList* io_data) {
  AudioOutputUnit* audio_unit = static_cast<AudioOutputUnit*>(in_ref_con);
  return audio_unit->NotifyGetPlayoutData(flags, time_stamp, bus_number, num_frames, io_data);
}

OSStatus AudioOutputUnit::NotifyGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                               const AudioTimeStamp* time_stamp,
                                               UInt32 bus_number,
                                               UInt32 num_frames,
                                               AudioBufferList* io_data) {
  return observer_->OnGetPlayoutData(flags, time_stamp, bus_number, num_frames, io_data);
}

}  // namespace ios_adm
}  // namespace webrtc
