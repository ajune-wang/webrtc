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

#include "webrtc/rtc_base/bitrateallocationstrategy.h"

@implementation ARDBitrateAllocationStrategy {
  rtc::AudioPriorityBitrateAllocationStrategy* _strategy;
}

+ (ARDBitrateAllocationStrategy*)
    createAudioPriorityBitrateAllocationStrategyForConfiguration:(RTCConfiguration*)configuration
                                                  withAudioTrack:(NSString*)audioTrackID
                                          sufficientAudioBitrate:(uint32_t)sufficientAudioBitrate {
  return [[ARDBitrateAllocationStrategy alloc] initWithConfiguration:configuration
                                                      withAudioTrack:audioTrackID
                                              sufficientAudioBitrate:sufficientAudioBitrate];
}

- (instancetype)initWithConfiguration:(RTCConfiguration*)configuration
                       withAudioTrack:(NSString*)audioTrackID
               sufficientAudioBitrate:(uint32_t)sufficientAudioBitrate {
  if (self = [super init]) {
    _strategy = new rtc::AudioPriorityBitrateAllocationStrategy(
        std::string(audioTrackID.UTF8String), sufficientAudioBitrate);
    configuration.bitrateAllocationStrategy =
        [[RTCBitrateAllocationStrategy alloc] initWith:_strategy];
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
