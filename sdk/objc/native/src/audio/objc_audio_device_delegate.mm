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
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"

#import "base/RTCLogging.h"
#import "components/audio/RTCAudioSessionConfiguration.h"

namespace {

class AudioDeviceDelegateImpl final : public rtc::RefCountedNonVirtual<AudioDeviceDelegateImpl> {
 public:
  AudioDeviceDelegateImpl(rtc::Thread* thread,
                          webrtc::objc_adm::ObjCAudioDevice* unowned_audio_device)
      : thread_(thread), unowned_audio_device_(unowned_audio_device) {
    RTC_DCHECK(thread_);
    RTC_DCHECK(unowned_audio_device_);
  }

  webrtc::objc_adm::ObjCAudioDevice* unowned_audio_device() const { return unowned_audio_device_; }

  rtc::Thread* thread() const { return thread_; }

  void reset_unowned_audio_device() { unowned_audio_device_ = nullptr; }

 private:
  rtc::Thread* thread_;
  webrtc::objc_adm::ObjCAudioDevice* unowned_audio_device_;
};

}  // namespace

@implementation ObjCAudioDeviceDelegate {
  rtc::scoped_refptr<AudioDeviceDelegateImpl> impl_;
}

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
    impl_ = rtc::make_ref_counted<AudioDeviceDelegateImpl>(thread, unownedAudioDevice);

    __block rtc::scoped_refptr<AudioDeviceDelegateImpl> playout_delegate = impl_;
    getPlayoutData_ = ^OSStatus(AudioUnitRenderActionFlags* _Nonnull actionFlags,
                                const AudioTimeStamp* _Nonnull timestamp,
                                NSInteger inputBusNumber,
                                UInt32 frameCount,
                                AudioBufferList* _Nonnull inputData) {
      webrtc::objc_adm::ObjCAudioDevice* audio_device = playout_delegate->unowned_audio_device();
      if (audio_device) {
        return audio_device->OnGetPlayoutData(
            actionFlags, timestamp, inputBusNumber, frameCount, inputData);
      } else {
        *actionFlags |= kAudioUnitRenderAction_OutputIsSilence;
        RTC_LOG(LS_VERBOSE) << "No alive audio device";
        return noErr;
      }
    };

    __block rtc::scoped_refptr<AudioDeviceDelegateImpl> record_delegate = impl_;
    deliverRecordedData_ =
        ^OSStatus(AudioUnitRenderActionFlags* _Nonnull actionFlags,
                  const AudioTimeStamp* _Nonnull timestamp,
                  NSInteger inputBusNumber,
                  UInt32 frameCount,
                  const AudioBufferList* _Nullable inputData,
                  RTC_OBJC_TYPE(RTCAudioDeviceRenderRecordedDataBlock) _Nullable renderBlock) {
          webrtc::objc_adm::ObjCAudioDevice* audio_device = record_delegate->unowned_audio_device();
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
  RTC_DCHECK_RUN_ON(impl_->thread());
  webrtc::objc_adm::ObjCAudioDevice* audio_deivce = impl_->unowned_audio_device();
  if (audio_deivce) {
    audio_deivce->HandleAudioParametersChange();
  }
}

- (void)notifyAudioInterrupted {
  RTC_DCHECK_RUN_ON(impl_->thread());
  webrtc::objc_adm::ObjCAudioDevice* audio_deivce = impl_->unowned_audio_device();
  if (audio_deivce) {
    audio_deivce->HandleAudioInterrupted();
  }
}

- (void)dispatchAsync:(dispatch_block_t)block {
  RTC_DCHECK(impl_->thread());
  impl_->thread()->PostTask([block] {
    @autoreleasepool {
      block();
    }
  });
}

- (void)resetAudioDevice {
  RTC_DCHECK_RUN_ON(impl_->thread());
  impl_->reset_unowned_audio_device();
}

@end
