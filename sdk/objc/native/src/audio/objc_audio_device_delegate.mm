/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "objc_audio_device.h"
#import "objc_audio_device_delegate.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

#import "base/RTCLogging.h"
#import "components/audio/RTCAudioSessionConfiguration.h"

@implementation ObjCAudioDeviceDelegate

@synthesize thread = thread_;

@synthesize unownedAudioDevice = unownedAudioDevice_;

@synthesize getPlayoutData = getPlayoutData_;

@synthesize deliverRecordedData = deliverRecordedData_;

@synthesize preferredInputSampleRate = preferredInputSampleRate_;

@synthesize preferredOutputSampleRate = preferredOutputSampleRate_;

@synthesize preferredInputIOBufferDuration = preferredInputIOBufferDuration_;

@synthesize preferredOutputIOBufferDuration = preferredOutputIOBufferDuration_;

- (instancetype)initWithUnownedAudioDevice:(webrtc::objc_adm::ObjCAudioDevice*)unownedAudioDevice
                         audioDeviceThread:(rtc::Thread*)thread {
  RTC_DCHECK_RUN_ON(thread);
  if (self = [super init]) {
    thread_ = thread;
    unownedAudioDevice_ = unownedAudioDevice;

    __weak __typeof__(self) weakSelf = self;
    getPlayoutData_ = ^OSStatus(AudioUnitRenderActionFlags* _Nonnull actionFlags,
                                const AudioTimeStamp* _Nonnull timestamp,
                                NSInteger inputBusNumber,
                                UInt32 frameCount,
                                AudioBufferList* _Nonnull inputData) {
      __strong __typeof__(weakSelf) self = weakSelf;
      if (!self) {
        return noErr;
      }

      auto audio_device = self.unownedAudioDevice;
      if (audio_device) {
        return audio_device->OnGetPlayoutData(
            actionFlags, timestamp, inputBusNumber, frameCount, inputData);
      } else {
        *actionFlags |= kAudioUnitRenderAction_OutputIsSilence;
        RTC_LOG(LS_VERBOSE) << "No alive audio device";
        return noErr;
      }
    };

    deliverRecordedData_ =
        ^OSStatus(AudioUnitRenderActionFlags* _Nonnull actionFlags,
                  const AudioTimeStamp* _Nonnull timestamp,
                  NSInteger inputBusNumber,
                  UInt32 frameCount,
                  const AudioBufferList* _Nullable inputData,
                  RTC_OBJC_TYPE(RTCAudioDeviceRenderRecordedDataBlock) _Nullable renderBlock) {
          __strong __typeof__(weakSelf) self = weakSelf;
          if (!self) {
            return noErr;
          }

          auto audio_device = self.unownedAudioDevice;
          if (audio_device) {
            return audio_device->OnDeliverRecordedData(
                actionFlags, timestamp, inputBusNumber, frameCount, inputData, renderBlock);
          } else {
            RTC_LOG(LS_VERBOSE) << "No alive audio device";
            return noErr;
          }
        };
    preferredInputSampleRate_ = kRTCAudioSessionHighPerformanceSampleRate;
    preferredOutputSampleRate_ = kRTCAudioSessionHighPerformanceSampleRate;
    preferredInputIOBufferDuration_ = kRTCAudioSessionHighPerformanceIOBufferDuration;
    preferredOutputIOBufferDuration_ = kRTCAudioSessionHighPerformanceIOBufferDuration;
  }
  return self;
}

- (void)notifyAudioParametersChange {
  RTC_DCHECK_RUN_ON(thread_);
  auto* audio_deivce = self.unownedAudioDevice;
  if (audio_deivce) {
    audio_deivce->HandleAudioParametersChange();
  }
}

- (void)notifyAudioInterrupted {
  RTC_DCHECK_RUN_ON(thread_);
  auto* audio_deivce = self.unownedAudioDevice;
  if (audio_deivce) {
    audio_deivce->HandleAudioInterrupted();
  }
}

- (void)dispatchAsync:(dispatch_block_t)block {
  RTC_DCHECK(thread_);
  thread_->PostTask([block] {
    @autoreleasepool {
      block();
    }
  });
}

- (void)resetAudioDevice {
  RTC_DCHECK_RUN_ON(thread_);
  unownedAudioDevice_ = nil;
}

@end
