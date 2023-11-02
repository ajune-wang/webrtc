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
#import <XCTest/XCTest.h>


#import "api/peerconnection/RTCDataChannelConfiguration+Private.h"
#import "api/peerconnection/RTCDataChannelConfiguration.h"
#import "helpers/NSString+StdString.h"

@interface RTCDataChannelConfigurationTest : XCTestCase
@end

@implementation RTCDataChannelConfigurationTest

- (void)testConversionToNativeDataChannelInit {
  BOOL isOrdered = NO;
  int maxPacketLifeTime = 5;
  int maxRetransmits = 4;
  BOOL isNegotiated = YES;
  int channelId = 4;
  NSString *protocol = @"protocol";

  RTC_OBJC_TYPE(RTCDataChannelConfiguration) *dataChannelConfig =
      [[RTC_OBJC_TYPE(RTCDataChannelConfiguration) alloc] init];
  dataChannelConfig.isOrdered = isOrdered;
  dataChannelConfig.maxPacketLifeTime = maxPacketLifeTime;
  dataChannelConfig.maxRetransmits = maxRetransmits;
  dataChannelConfig.isNegotiated = isNegotiated;
  dataChannelConfig.channelId = channelId;
  dataChannelConfig.protocol = protocol;

  webrtc::DataChannelInit nativeInit = dataChannelConfig.nativeDataChannelInit;
  XCTAssertEqual(isOrdered, nativeInit.ordered);
  XCTAssertEqual(maxPacketLifeTime, nativeInit.maxRetransmitTime);
  XCTAssertEqual(maxRetransmits, nativeInit.maxRetransmits);
  XCTAssertEqual(isNegotiated, nativeInit.negotiated);
  XCTAssertEqual(channelId, nativeInit.id);
  XCTAssertEqual(protocol.stdString, nativeInit.protocol);
}

@end
