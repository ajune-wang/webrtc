/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactoryBuilder.h"
#import "RTCPeerConnectionFactory+Native.h"

#ifndef HAVE_NO_MEDIA
// The no-media version PeerConnectionFactory doesn't depend on these files, but the gn check tool
// is not smart enough to take the #ifdef into account.
#include "api/audio_codecs/builtin_audio_decoder_factory.h"     // nogncheck
#include "api/audio_codecs/builtin_audio_encoder_factory.h"     // nogncheck
#include "api/video_codecs/video_decoder_factory.h"             // nogncheck
#include "api/video_codecs/video_encoder_factory.h"             // nogncheck
#include "modules/audio_device/include/audio_device.h"          // nogncheck
#include "modules/audio_processing/include/audio_processing.h"  // nogncheck
#endif

#if defined(WEBRTC_IOS)
#import "sdk/objc/Framework/Native/api/audio_device_module.h"
#endif

@implementation RTCPeerConnectionFactoryBuilder {
  std::unique_ptr<webrtc::VideoEncoderFactory> _videoEncoderFactory;
  std::unique_ptr<webrtc::VideoDecoderFactory> _videoDecoderFactory;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> _audioEncoderFactory;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> _audioDecoderFactory;
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _audioDeviceModule;
  rtc::scoped_refptr<webrtc::AudioProcessing> _audioProcessingModule;
}

- (void)setVideoEncoderFactory:(std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory {
  _videoEncoderFactory = std::move(videoEncoderFactory);
}

- (void)setVideoDecoderFactory:(std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory {
  _videoDecoderFactory = std::move(videoDecoderFactory);
}

- (void)setAudioEncoderFactory:
        (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory {
  _audioEncoderFactory = audioEncoderFactory;
}

- (void)setAudioDecoderFactory:
        (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory {
  _audioDecoderFactory = audioDecoderFactory;
}

- (void)setAudioDeviceModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule {
  _audioDeviceModule = audioDeviceModule;
}

- (void)setAudioProcessingModule:
        (rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule {
  _audioProcessingModule = audioProcessingModule;
}

- (RTCPeerConnectionFactory *)createPeerConnectionFactory {
  return [[RTCPeerConnectionFactory alloc]
      initWithNativeAudioEncoderFactory:_audioEncoderFactory
              nativeAudioDecoderFactory:_audioDecoderFactory
              nativeVideoEncoderFactory:std::move(_videoEncoderFactory)
              nativeVideoDecoderFactory:std::move(_videoDecoderFactory)
                      audioDeviceModule:_audioDeviceModule
                  audioProcessingModule:_audioProcessingModule];
}

@end
