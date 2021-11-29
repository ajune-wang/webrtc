/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_INPUT_UNIT_H_
#define SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_INPUT_UNIT_H_

#include <AudioUnit/AudioUnit.h>

#include "base_audio_unit.h"

namespace webrtc {
namespace ios_adm {

class AudioInputUnitObserver {
 public:
  // Callback function called on a real-time priority I/O thread from the audio
  // unit. This method is used to signal that recorded audio is available.
  virtual OSStatus OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                         const AudioTimeStamp* time_stamp,
                                         UInt32 bus_number,
                                         UInt32 num_frames,
                                         AudioBufferList* io_data) = 0;

 protected:
  ~AudioInputUnitObserver() {}
};

// Convenience class to abstract away the management of a Voice Processing
// I/O Audio Unit. The Voice Processing I/O unit has the same characteristics
// as the Remote I/O unit (supports full duplex low-latency audio input and
// output) and adds AEC for for two-way duplex communication. It also adds AGC,
// adjustment of voice-processing quality, and muting. Hence, ideal for
// VoIP applications.
class AudioInputUnit final : public BaseAudioUnit {
 public:
  AudioInputUnit(bool bypass_voice_processing,
                 AudioInputUnitObserver* observer);
  virtual ~AudioInputUnit() override = default;

  // Calls render on the underlying audio unit.
  OSStatus Render(AudioUnitRenderActionFlags* flags,
                  const AudioTimeStamp* time_stamp,
                  UInt32 output_bus_number,
                  UInt32 num_frames,
                  AudioBufferList* io_data);

 protected:
  // Called during Init execution and suppoed to find, configure & instanticate
  // audio unit.
  virtual BaseAudioUnit::OwnedAudioUnit InstantiateAudioUnit() override;

 private:
  // The C API used to set callbacks requires static functions. When these are
  // called, they will invoke the relevant instance method by casting
  // in_ref_con to AudioInputUnit*.
  static OSStatus OnDeliverRecordedData(void* in_ref_con,
                                        AudioUnitRenderActionFlags* flags,
                                        const AudioTimeStamp* time_stamp,
                                        UInt32 bus_number,
                                        UInt32 num_frames,
                                        AudioBufferList* io_data);

  // Notifies observer that recorded samples are available for render.
  OSStatus NotifyDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                     const AudioTimeStamp* time_stamp,
                                     UInt32 bus_number,
                                     UInt32 num_frames,
                                     AudioBufferList* io_data);

  const bool bypass_voice_processing_;
  AudioInputUnitObserver* observer_;
};
}  // namespace ios_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_INPUT_UNIT_H_
