/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
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
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"
#import "sdk/objc/components/audio/RTCAudioSessionConfiguration.h"

#if !defined(NDEBUG)
static void LogStreamDescription(AudioStreamBasicDescription description) {
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

namespace webrtc {
namespace ios_adm {

// Calls to AudioUnitInitialize() can fail if called back-to-back on different
// ADM instances. A fall-back solution is to allow multiple sequential calls
// with as small delay between each. This factor sets the max number of allowed
// initialization attempts.
static const int kMaxNumberOfAudioUnitInitializeAttempts = 5;

// A Remote I/O unit's bus 0 connects to output hardware (speaker).
static const AudioUnitElement kOutputBus = 0;

AudioOutputUnit::AudioOutputUnit(AudioOutputUnitObserver* observer)
    : observer_(observer), remoteio_unit_(nullptr), state_(State::kInitRequired) {
  RTC_DCHECK(observer);
}

AudioOutputUnit::~AudioOutputUnit() {
  DisposeAudioUnit();
}

const UInt32 AudioOutputUnit::kBytesPerSample = 2;

bool AudioOutputUnit::Init() {
  RTC_DCHECK_EQ(state_, State::kInitRequired);

  // Create an audio component description to identify RemoteIO audio unit.
  AudioComponentDescription remoteio_unit = {
      .componentType = kAudioUnitType_Output,
      .componentSubType = kAudioUnitSubType_RemoteIO,
      .componentManufacturer = kAudioUnitManufacturer_Apple,
      .componentFlags = 0,
      .componentFlagsMask = 0,
  };

  // Obtain an audio unit instance given the description.
  AudioComponent found_remoteio_unit_ref = AudioComponentFindNext(nullptr, &remoteio_unit);

  // Create a Voice Processing IO audio unit.
  OSStatus result = noErr;
  result = AudioComponentInstanceNew(found_remoteio_unit_ref, &remoteio_unit_);
  if (result != noErr) {
    remoteio_unit_ = nullptr;
    RTCLogError(@"AudioComponentInstanceNew failed. Error=%ld.", (long)result);
    return false;
  }

  // Enable output on the output scope of the output element.
  UInt32 enable_output = 1;
  result = AudioUnitSetProperty(remoteio_unit_,
                                kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output,
                                kOutputBus,
                                &enable_output,
                                sizeof(enable_output));
  if (result != noErr) {
    DisposeAudioUnit();
    RTCLogError(@"Failed to enable output on output scope of output element. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  // Specify the callback function that provides audio samples to the audio
  // unit.
  AURenderCallbackStruct render_callback;
  render_callback.inputProc = OnGetPlayoutData;
  render_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(remoteio_unit_,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                kOutputBus,
                                &render_callback,
                                sizeof(render_callback));
  if (result != noErr) {
    DisposeAudioUnit();
    RTCLogError(@"Failed to specify the render callback on the output bus. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  state_ = State::kUninitialized;
  return true;
}

AudioOutputUnit::State AudioOutputUnit::GetState() const {
  return state_;
}

bool AudioOutputUnit::Initialize(Float64 sample_rate) {
  RTC_DCHECK_GE(state_, State::kUninitialized);
  RTCLog(@"Initializing audio unit with sample rate: %f", sample_rate);

  OSStatus result = noErr;
  AudioStreamBasicDescription format = GetFormat(sample_rate);
  UInt32 size = sizeof(format);
#if !defined(NDEBUG)
  LogStreamDescription(format);
#endif

  // Set the format on the input scope of the output element/bus.
  result = AudioUnitSetProperty(remoteio_unit_,
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
  result = AudioUnitInitialize(remoteio_unit_);
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
    result = AudioUnitInitialize(remoteio_unit_);
  }
  if (result == noErr) {
    RTCLog(@"Remote I/O unit is now initialized.");
  }

  state_ = State::kInitialized;
  return true;
}

OSStatus AudioOutputUnit::Start() {
  RTC_DCHECK_GE(state_, State::kUninitialized);
  RTCLog(@"Starting audio unit.");

  OSStatus result = AudioOutputUnitStart(remoteio_unit_);
  if (result != noErr) {
    RTCLogError(@"Failed to start audio unit. Error=%ld", (long)result);
    return result;
  } else {
    RTCLog(@"Started audio unit");
  }
  state_ = State::kStarted;
  return noErr;
}

bool AudioOutputUnit::Stop() {
  RTC_DCHECK_GE(state_, State::kUninitialized);
  RTCLog(@"Stopping audio unit.");

  OSStatus result = AudioOutputUnitStop(remoteio_unit_);
  if (result != noErr) {
    RTCLogError(@"Failed to stop audio unit. Error=%ld", (long)result);
    return false;
  } else {
    RTCLog(@"Stopped audio unit");
  }

  state_ = State::kInitialized;
  return true;
}

bool AudioOutputUnit::Uninitialize() {
  RTC_DCHECK_GE(state_, State::kUninitialized);
  RTCLog(@"Unintializing audio unit.");

  OSStatus result = AudioUnitUninitialize(remoteio_unit_);
  if (result != noErr) {
    RTCLogError(@"Failed to uninitialize audio unit. Error=%ld", (long)result);
    return false;
  } else {
    RTCLog(@"Uninitialized audio unit.");
  }

  state_ = State::kUninitialized;
  return true;
}

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

AudioStreamBasicDescription AudioOutputUnit::GetFormat(Float64 sample_rate) const {
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

void AudioOutputUnit::DisposeAudioUnit() {
  if (remoteio_unit_) {
    switch (state_) {
      case State::kStarted:
        Stop();
        // Fall through.
        ABSL_FALLTHROUGH_INTENDED;
      case State::kInitialized:
        Uninitialize();
        break;
      case State::kUninitialized:
        ABSL_FALLTHROUGH_INTENDED;
      case State::kInitRequired:
        break;
    }

    RTCLog(@"Disposing audio unit.");
    OSStatus result = AudioComponentInstanceDispose(remoteio_unit_);
    if (result != noErr) {
      RTCLogError(@"AudioComponentInstanceDispose failed. Error=%ld.", (long)result);
    }
    remoteio_unit_ = nullptr;
  }
}

}  // namespace ios_adm
}  // namespace webrtc
