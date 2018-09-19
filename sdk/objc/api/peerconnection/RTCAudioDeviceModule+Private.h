/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioDeviceModule.h"

#if defined(WEBRTC_IOS)
#import "sdk/objc/native/api/audio_device_module.h"
#endif

NS_ASSUME_NONNULL_BEGIN

@interface RTCAudioDeviceModule ()

@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::AudioDeviceModule>
    nativeAudioDeviceModule;

+ (rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule;

@end

NS_ASSUME_NONNULL_END
