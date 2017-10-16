/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDBitrateAllocationStrategy.h"
#import "WebRTC/RTCBitrateAllocationStrategy.h"

#include "rtc_base/bitrateallocationstrategy.h"

@implementation ARDBitrateAllocationStrategy {
  rtc::AudioPriorityBitrateAllocationStrategy* _strategy;
}

+ (ARDBitrateAllocationStrategy*)
    createAudioPriorityBitrateAllocationStrategyForPeerConnection:(RTCPeerConnection*)peerConnection
                                                   withAudioTrack:(NSString*)audioTrackID
                                           sufficientAudioBitrate:(uint32_t)sufficientAudioBitrate {
  return [[ARDBitrateAllocationStrategy alloc] initWithPeerCoonnection:peerConnection
                                                        withAudioTrack:audioTrackID
                                                sufficientAudioBitrate:sufficientAudioBitrate];
}

- (instancetype)initWithPeerCoonnection:(RTCPeerConnection*)peerConnection
                         withAudioTrack:(NSString*)audioTrackID
                 sufficientAudioBitrate:(uint32_t)sufficientAudioBitrate {
  if (self = [super init]) {
    _strategy = new rtc::AudioPriorityBitrateAllocationStrategy(
        std::string(audioTrackID.UTF8String), sufficientAudioBitrate);
    [peerConnection
        setBitrateAllocationStrategy:[[RTCBitrateAllocationStrategy alloc]
                                         initWith:new rtc::AudioPriorityBitrateAllocationStrategy(
                                                      std::string(audioTrackID.UTF8String),
                                                      sufficientAudioBitrate)]];
  }
  return self;
}

- (void)dealloc {
  if (_strategy) {
    delete _strategy;
    _strategy = nullptr;
  }
}

@end
