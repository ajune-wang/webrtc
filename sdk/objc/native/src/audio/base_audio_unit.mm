/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "base_audio_unit.h"

#include "rtc_base/checks.h"
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"
#import "sdk/objc/components/audio/RTCAudioSessionConfiguration.h"

namespace webrtc {
namespace ios_adm {

// Calls to AudioUnitInitialize() can fail if called back-to-back on different
// ADM instances. A fall-back solution is to allow multiple sequential calls
// with as small delay between each. This factor sets the max number of allowed
// initialization attempts.
static const int kMaxNumberOfAudioUnitInitializeAttempts = 5;

BaseAudioUnit::~BaseAudioUnit() {
  DisposeAudioUnit();
}

const UInt32 BaseAudioUnit::kBytesPerSample = 2;

const AudioUnitElement BaseAudioUnit::kOutputBus = 0;

const AudioUnitElement BaseAudioUnit::kInputBus = 1;

bool BaseAudioUnit::Init() {
  RTC_DCHECK_EQ(state_, State::kInitRequired);

  OwnedAudioUnit audio_unit = InstantiateAudioUnit();
  if (audio_unit == nullptr) {
    RTCLogError(@"Unable to instantiate audio unit.");
    return false;
  }

  audio_unit_ = std::move(audio_unit);
  state_ = State::kUninitialized;
  return true;
}

BaseAudioUnit::State BaseAudioUnit::GetState() const {
  return state_;
}

bool BaseAudioUnit::Initialize(Float64 sample_rate) {
  RTC_DCHECK_EQ(state_, State::kUninitialized);
  RTCLog(@"Initializing audio unit with sample rate: %f", sample_rate);

  OSStatus result = noErr;
  AudioStreamBasicDescription format = GetFormat(sample_rate);
  UInt32 size = sizeof(format);
#if !defined(NDEBUG)
  LogStreamDescription(format);
#endif

  // Set the format on the output scope of the input element/bus.
  result = AudioUnitSetProperty(audio_unit_.get(),
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output,
                                kInputBus,
                                &format,
                                size);
  if (result != noErr) {
    RTCLogError(@"Failed to set format on output scope of input bus. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  // Set the format on the input scope of the output element/bus.
  result = AudioUnitSetProperty(audio_unit_.get(),
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                kOutputBus,
                                &format,
                                size);
  if (result != noErr) {
    RTCLogError(@"Failed to set format on input scope of output bus. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  // Initialize the Remote I/O unit instance.
  // Calls to AudioUnitInitialize() can fail if called back-to-back on
  // different ADM instances. The error message in this case is -66635 which is
  // undocumented. Tests have shown that calling AudioUnitInitialize a second
  // time, after a short sleep, avoids this issue.
  // See webrtc:5166 for details.
  int failed_initalize_attempts = 0;
  result = AudioUnitInitialize(audio_unit_.get());
  while (result != noErr) {
    RTCLogError(@"Failed to initialize the Remote I/O unit. "
                 "Error=%ld.",
                (long)result);
    ++failed_initalize_attempts;
    if (failed_initalize_attempts == kMaxNumberOfAudioUnitInitializeAttempts) {
      // Max number of initialization attempts exceeded, hence abort.
      RTCLogError(@"Too many initialization attempts.");
      return false;
    }
    RTCLog(@"Pause 100ms and try audio unit initialization again...");
    [NSThread sleepForTimeInterval:0.1f];
    result = AudioUnitInitialize(audio_unit_.get());
  }
  if (result == noErr) {
    RTCLog(@"Audio unit is now initialized.");
  }

  state_ = State::kInitialized;

  UpdatePropertiesPostInitialize();

  return true;
}

OSStatus BaseAudioUnit::Start() {
  RTC_DCHECK_EQ(state_, State::kInitialized);
  RTCLog(@"Starting audio unit.");

  OSStatus result = AudioOutputUnitStart(audio_unit_.get());
  if (result != noErr) {
    RTCLogError(@"Failed to start audio unit. Error=%ld", (long)result);
    return result;
  } else {
    RTCLog(@"Started audio unit");
  }
  state_ = State::kStarted;
  return noErr;
}

bool BaseAudioUnit::Stop() {
  RTC_DCHECK_EQ(state_, State::kStarted);
  RTCLog(@"Stopping audio unit.");

  OSStatus result = AudioOutputUnitStop(audio_unit_.get());
  if (result != noErr) {
    RTCLogError(@"Failed to stop audio unit. Error=%ld", (long)result);
    return false;
  } else {
    RTCLog(@"Stopped audio unit");
  }

  state_ = State::kInitialized;
  return true;
}

bool BaseAudioUnit::Uninitialize() {
  RTC_DCHECK_GE(state_, State::kUninitialized);
  RTCLog(@"Unintializing audio unit.");

  OSStatus result = AudioUnitUninitialize(audio_unit_.get());
  if (result != noErr) {
    RTCLogError(@"Failed to uninitialize audio unit. Error=%ld", (long)result);
    return false;
  } else {
    RTCLog(@"Uninitialized audio unit.");
  }

  state_ = State::kUninitialized;
  return true;
}

AudioStreamBasicDescription BaseAudioUnit::GetFormat(Float64 sample_rate) const {
  // Set the application formats for input and output:
  // - use same format in both directions
  // - avoid resampling in the I/O unit by using the hardware sample rate
  // - linear PCM => noncompressed audio data format with one frame per packet
  // - no need to specify interleaving since only mono is supported
  AudioStreamBasicDescription format;
  RTC_DCHECK_EQ(1, kRTCAudioSessionPreferredNumberOfChannels);
  format.mSampleRate = sample_rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  format.mBytesPerPacket = kBytesPerSample;
  format.mFramesPerPacket = 1;  // uncompressed.
  format.mBytesPerFrame = kBytesPerSample;
  format.mChannelsPerFrame = kRTCAudioSessionPreferredNumberOfChannels;
  format.mBitsPerChannel = 8 * kBytesPerSample;
  return format;
}

void BaseAudioUnit::DisposeAudioUnit() {
  if (audio_unit_) {
    RTCLog(@"Disposing audio unit in state %d", state_);
    switch (state_) {
      case State::kStarted:
        Stop();
        // Fall through.
        [[fallthrough]];
      case State::kInitialized:
        Uninitialize();
        break;
      case State::kUninitialized:
        [[fallthrough]];
      case State::kInitRequired:
        break;
    }

    audio_unit_.reset();
  }
}

#if !defined(NDEBUG)
void BaseAudioUnit::LogStreamDescription(AudioStreamBasicDescription description) {
  char formatIdString[5];
  UInt32 formatId = CFSwapInt32HostToBig(description.mFormatID);
  bcopy(&formatId, formatIdString, 4);
  formatIdString[4] = '\0';
  RTCLog(@"AudioStreamBasicDescription: {\n"
          "  mSampleRate: %.2f\n"
          "  formatIDString: %s\n"
          "  mFormatFlags: 0x%X\n"
          "  mBytesPerPacket: %u\n"
          "  mFramesPerPacket: %u\n"
          "  mBytesPerFrame: %u\n"
          "  mChannelsPerFrame: %u\n"
          "  mBitsPerChannel: %u\n"
          "  mReserved: %u\n}",
         description.mSampleRate,
         formatIdString,
         static_cast<unsigned int>(description.mFormatFlags),
         static_cast<unsigned int>(description.mBytesPerPacket),
         static_cast<unsigned int>(description.mFramesPerPacket),
         static_cast<unsigned int>(description.mBytesPerFrame),
         static_cast<unsigned int>(description.mChannelsPerFrame),
         static_cast<unsigned int>(description.mBitsPerChannel),
         static_cast<unsigned int>(description.mReserved));
}
#endif

void BaseAudioUnit::AudioUnitDeleter::operator()(AudioUnit unit) const {
  OSStatus result = AudioComponentInstanceDispose(unit);
  if (result != noErr) {
    RTCLogError(@"AudioComponentInstanceDispose failed. Error=%ld.", (long)result);
  }
}

}  // namespace ios_adm
}  // namespace webrtc
