/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioDeviceModule+Private.h"

@implementation RTCAudioDeviceModule

- (instancetype)init {
  if (self = [super init]) {
    _nativeAudioDeviceModule = [RTCAudioDeviceModule audioDeviceModule];
  }
  return self;
}

- (void)setSpeakerMute:(bool)enable {
  if (self.nativeAudioDeviceModule) {
    self.nativeAudioDeviceModule->SetSpeakerMute(enable);
  }
}

- (void)setMicrophoneMute:(bool)enable {
  if (self.nativeAudioDeviceModule) {
    self.nativeAudioDeviceModule->SetMicrophoneMute(enable);
  }
}

+ (rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule {
#if defined(WEBRTC_IOS)
  return webrtc::CreateAudioDeviceModule();
#else
  return nullptr;
#endif
}

@end
