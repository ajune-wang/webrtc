/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_AUDIO_DEVICE_DELEGATE_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_AUDIO_DEVICE_DELEGATE_H_

#include "rtc_base/thread.h"

#import "RTCAudioDevice.h"

#include <AudioUnit/AudioUnit.h>

namespace webrtc {
namespace objc_adm {
class ObjCAudioDevice;
}
}  // namespace webrtc

@interface ObjCAudioDeviceDelegate : NSObject <RTC_OBJC_TYPE (RTCAudioDeviceDelegate)>

- (instancetype)initWithUnownedAudioDevice:(webrtc::objc_adm::ObjCAudioDevice*)unownedAudioDevice
                         audioDeviceThread:(rtc::Thread*)thread;

@property(readonly) rtc::Thread* thread;

@property(readonly) webrtc::objc_adm::ObjCAudioDevice* unownedAudioDevice;

- (void)resetAudioDevice;

@end

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_AUDIO_DEVICE_DELEGATE_H_
