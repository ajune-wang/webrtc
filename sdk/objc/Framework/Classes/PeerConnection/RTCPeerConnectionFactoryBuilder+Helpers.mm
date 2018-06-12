/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactory+Native.h"
#import "RTCPeerConnectionFactoryBuilder+Helpers.h"

#ifndef HAVE_NO_MEDIA
#import "WebRTC/RTCVideoCodecH264.h"
// The no-media version PeerConnectionFactory doesn't depend on these files, but the gn check tool
// is not smart enough to take the #ifdef into account.
#include "api/audio_codecs/builtin_audio_decoder_factory.h"     // nogncheck
#include "api/audio_codecs/builtin_audio_encoder_factory.h"     // nogncheck
#include "modules/audio_device/include/audio_device.h"          // nogncheck
#include "modules/audio_processing/include/audio_processing.h"  // nogncheck

#include "sdk/objc/Framework/Native/api/video_decoder_factory.h"
#include "sdk/objc/Framework/Native/api/video_encoder_factory.h"
#include "sdk/objc/Framework/Native/src/objc_video_decoder_factory.h"
#include "sdk/objc/Framework/Native/src/objc_video_encoder_factory.h"
#endif

#if defined(WEBRTC_IOS)
#import "sdk/objc/Framework/Native/api/audio_device_module.h"
#endif

@implementation RTCPeerConnectionFactoryBuilder (Helpers)

+ (RTCPeerConnectionFactoryBuilder *)builder {
  RTCPeerConnectionFactoryBuilder *builder = [[RTCPeerConnectionFactoryBuilder alloc] init];
  auto audioEncoderFactory = webrtc::CreateBuiltinAudioEncoderFactory();
  [builder setAudioEncoderFactory:audioEncoderFactory];

  auto audioDecoderFactory = webrtc::CreateBuiltinAudioDecoderFactory();
  [builder setAudioDecoderFactory:audioDecoderFactory];

  auto videoEncoderFactory =
      webrtc::ObjCToNativeVideoEncoderFactory([[RTCVideoEncoderFactoryH264 alloc] init]);
  [builder setVideoEncoderFactory:std::move(videoEncoderFactory)];

  auto videoDecoderFactory =
      webrtc::ObjCToNativeVideoDecoderFactory([[RTCVideoDecoderFactoryH264 alloc] init]);
  [builder setVideoDecoderFactory:std::move(videoDecoderFactory)];

#if defined(WEBRTC_IOS)
  [builder setAudioDeviceModule:webrtc::CreateAudioDeviceModule()];
#endif
  return builder;
}

+ (RTCPeerConnectionFactoryBuilder *)builderWithAudioDeviceModule:
        (rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule {
  RTCPeerConnectionFactoryBuilder *builder = [RTCPeerConnectionFactoryBuilder builder];
  [builder setAudioDeviceModule:audioDeviceModule];
  return builder;
}

@end
