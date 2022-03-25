/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_OUTPUT_UNIT_H_
#define SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_OUTPUT_UNIT_H_

#include <AudioUnit/AudioUnit.h>

#include "base_audio_unit.h"

namespace webrtc {
namespace ios_adm {

class AudioOutputUnitObserver {
 public:
  // Callback function called on a real-time priority I/O thread from the audio
  // unit. This method is used to provide audio samples to the audio unit.
  virtual OSStatus OnGetPlayoutData(AudioUnitRenderActionFlags* io_action_flags,
                                    const AudioTimeStamp* time_stamp,
                                    UInt32 bus_number,
                                    UInt32 num_frames,
                                    AudioBufferList* io_data) = 0;

 protected:
  ~AudioOutputUnitObserver() {}
};

// Convenience class to abstract away the management of audio unit
// used for audio playout.
class AudioOutputUnit final : public BaseAudioUnit {
 public:
  enum class UnitType {
    kRemoteIO,
    // Voice-Processing IO audio unit will be configured in output-only mode.
    // Voice-Processing IO might be preferred over Remote IO unit because
    // according to experiment VPIO unit ducks Remote IO unit, but not
    // other instance of VPIO unit.
    kVoiceProcessingIO,
  };

  AudioOutputUnit(AudioOutputUnit::UnitType type,
                  AudioOutputUnitObserver* observer);
  virtual ~AudioOutputUnit() override = default;

 protected:
  // Called during Init execution and supposed to find, configure & instanticate
  // audio unit.
  virtual BaseAudioUnit::OwnedAudioUnit InstantiateAudioUnit() override;

  virtual void UpdatePropertiesPostInitialize() override;

 private:
  // The C API used to set callbacks requires static functions. When these are
  // called, they will invoke the relevant instance method by casting
  // in_ref_con to AudioOutputUnit*.
  static OSStatus OnGetPlayoutData(void* in_ref_con,
                                   AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* time_stamp,
                                   UInt32 bus_number,
                                   UInt32 num_frames,
                                   AudioBufferList* io_data);

  // Notifies observer that samples are needed for playback.
  OSStatus NotifyGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                const AudioTimeStamp* time_stamp,
                                UInt32 bus_number,
                                UInt32 num_frames,
                                AudioBufferList* io_data);

  AudioOutputUnitObserver* observer_;
  UnitType audio_unit_type_;
};
}  // namespace ios_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_OUTPUT_UNIT_H_
