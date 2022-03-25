/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_AUDIO_BASE_AUDIO_UNIT_H_
#define SDK_OBJC_NATIVE_SRC_AUDIO_BASE_AUDIO_UNIT_H_

#include <AudioUnit/AudioUnit.h>

#include <memory>

namespace webrtc {
namespace ios_adm {

// Convenience class to abstract away the management of generic audio unit
class BaseAudioUnit {
 public:
  virtual ~BaseAudioUnit();

  // TODO(tkchin): enum for state and state checking.
  enum class State {
    // Init() should be called.
    kInitRequired,
    // Audio unit created but not initialized.
    kUninitialized,
    // Initialized but not started. Equivalent to stopped.
    kInitialized,
    // Initialized and started.
    kStarted,
  };

  // Number of bytes per audio sample for 16-bit signed integer representation.
  static const UInt32 kBytesPerSample;

  // An audio unit's bus which connects to output hardware (e.g. speaker).
  static const AudioUnitElement kOutputBus;

  // An audio unit's bus which connects to input hardware (e.g. microphone).
  static const AudioUnitElement kInputBus;

  // Initializes this class by creating the underlying audio unit instance.
  // Creates and configures audio unit.
  // The selected stream format is selected to avoid internal resampling
  // and to match the 10ms callback rate for WebRTC as well as possible.
  // Does not initialize the audio unit.
  bool Init();

  BaseAudioUnit::State GetState() const;

  // Initializes the underlying audio unit with the given sample rate.
  bool Initialize(Float64 sample_rate);

  // Starts the underlying audio unit.
  OSStatus Start();

  // Stops the underlying audio unit.
  bool Stop();

  // Uninitializes the underlying audio unit.
  bool Uninitialize();

 protected:
  struct AudioUnitDeleter {
    void operator()(AudioUnit unit) const;
  };

  typedef std::unique_ptr<std::remove_pointer<AudioUnit>::type,
                          AudioUnitDeleter>
      OwnedAudioUnit;

  // Called during Init execution and supposed to find, configure & instanticate
  // audio unit.
  virtual OwnedAudioUnit InstantiateAudioUnit() = 0;

  // Called after Initialize done it's job. Usefull for properties which could
  // only be updated after initialization of audio unit
  virtual void UpdatePropertiesPostInitialize() = 0;

  // Returns the predetermined format with a specific sample rate. See
  // implementation file for details on format.
  AudioStreamBasicDescription GetFormat(Float64 sample_rate) const;

  // Deletes the underlying audio unit.
  void DisposeAudioUnit();

  AudioUnit audio_unit() const { return audio_unit_.get(); }

#if !defined(NDEBUG)
  static void LogStreamDescription(AudioStreamBasicDescription description);
#endif

 private:
  OwnedAudioUnit audio_unit_{nullptr};
  State state_{State::kInitRequired};
};
}  // namespace ios_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_BASE_AUDIO_UNIT_H_
