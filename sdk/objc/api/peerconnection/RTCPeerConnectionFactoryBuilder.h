/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactory.h"

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCPeerConnectionFactoryBuilder : NSObject

+ (RTCPeerConnectionFactoryBuilder *)builder;

- (RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)createPeerConnectionFactory;

- (void)setVideoEncoderFactory:(std::unique_ptr<webrtc::VideoEncoderFactory>)videoEncoderFactory;

- (void)setVideoDecoderFactory:(std::unique_ptr<webrtc::VideoDecoderFactory>)videoDecoderFactory;

- (void)setAudioEncoderFactory:(rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory;

- (void)setAudioDecoderFactory:(rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory;

- (void)setAudioDeviceModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule>)audioDeviceModule;

- (void)setAudioProcessingModule:(rtc::scoped_refptr<webrtc::AudioProcessing>)audioProcessingModule;

@end

NS_ASSUME_NONNULL_END
