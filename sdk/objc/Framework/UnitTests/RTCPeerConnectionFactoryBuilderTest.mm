/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>
#import "sdk/objc/Framework/Classes/PeerConnection/RTCPeerConnectionFactory+Native.h"
#import "sdk/objc/Framework/Classes/PeerConnection/RTCPeerConnectionFactoryBuilder.h"

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

#include "rtc_base/gunit.h"

@interface RTCPeerConnectionFactoryBuilderTest : NSObject
- (void)testDefaultBuilder;
@end

@implementation RTCPeerConnectionFactoryBuilderTest

- (void)testDefaultBuilder {
  RTCPeerConnectionFactoryBuilder *builder = [[RTCPeerConnectionFactoryBuilder alloc] init];

  [builder setAudioEncoderFactory:webrtc::CreateBuiltinAudioEncoderFactory()];
  [builder setAudioDecoderFactory:webrtc::CreateBuiltinAudioDecoderFactory()];
  RTCPeerConnectionFactory *peerConnectionFactory = [builder createPeerConnectionFactory];
  EXPECT_TRUE(peerConnectionFactory != nil);
}

@end

TEST(RTCPeerConnectionFactoryBuilderTest, DefaultBuilderTest) {
  @autoreleasepool {
    RTCPeerConnectionFactoryBuilderTest *test = [[RTCPeerConnectionFactoryBuilderTest alloc] init];
    [test testDefaultBuilder];
  }
}
