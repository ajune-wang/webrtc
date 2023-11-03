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

#include <memory>

#import "api/peerconnection/RTCIceCandidate+Private.h"
#import "api/peerconnection/RTCIceCandidate.h"
#import "helpers/NSString+StdString.h"

@interface RTCIceCandidateTest : XCTestCase
@end

@implementation RTCIceCandidateTest

- (void)testCandidate {
  NSString *sdp = @"candidate:4025901590 1 udp 2122265343 "
                   "fdff:2642:12a6:fe38:c001:beda:fcf9:51aa "
                   "59052 typ host generation 0";

  RTC_OBJC_TYPE(RTCIceCandidate) *candidate =
      [[RTC_OBJC_TYPE(RTCIceCandidate) alloc] initWithSdp:sdp sdpMLineIndex:0 sdpMid:@"audio"];

  std::unique_ptr<webrtc::IceCandidateInterface> nativeCandidate =
      candidate.nativeCandidate;
  XCTAssertEqual("audio", nativeCandidate->sdp_mid());
  XCTAssertEqual(0, nativeCandidate->sdp_mline_index());

  std::string sdpString;
  nativeCandidate->ToString(&sdpString);
  XCTAssertEqual(sdp.stdString, sdpString);
}

- (void)testInitFromNativeCandidate {
  std::string sdp("candidate:4025901590 1 udp 2122265343 "
                  "fdff:2642:12a6:fe38:c001:beda:fcf9:51aa "
                  "59052 typ host generation 0");
  std::unique_ptr<webrtc::IceCandidateInterface> nativeCandidate(
      webrtc::CreateIceCandidate("audio", 0, sdp, nullptr));

  RTC_OBJC_TYPE(RTCIceCandidate) *iceCandidate =
      [[RTC_OBJC_TYPE(RTCIceCandidate) alloc] initWithNativeCandidate:nativeCandidate.get()];
  std::unique_ptr<webrtc::IceCandidateInterface> iceCandidateFromObjc(iceCandidate.nativeCandidate);
  XCTAssertNotEqual(nativeCandidate.get(), iceCandidateFromObjc.get());
  XCTAssertTrue([@"audio" isEqualToString:iceCandidate.sdpMid]);
  XCTAssertEqual(0, iceCandidate.sdpMLineIndex);

  XCTAssertEqual(sdp, iceCandidate.sdp.stdString);
}

@end
