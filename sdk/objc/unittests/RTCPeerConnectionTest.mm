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
#include <vector>

#import "api/peerconnection/RTCConfiguration+Private.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCCryptoOptions.h"
#import "api/peerconnection/RTCIceCandidate.h"
#import "api/peerconnection/RTCIceServer.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory+Native.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "api/peerconnection/RTCSessionDescription.h"
#import "helpers/NSString+StdString.h"

@interface RTCPeerConnectionTests : XCTestCase
@end

@implementation RTCPeerConnectionTests

- (void)testConfigurationGetter {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTC_OBJC_TYPE(RTCIceServer) *server =
      [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithURLStrings:urlStrings];

  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
  config.iceServers = @[ server ];
  config.iceTransportPolicy = RTCIceTransportPolicyRelay;
  config.bundlePolicy = RTCBundlePolicyMaxBundle;
  config.rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
  config.tcpCandidatePolicy = RTCTcpCandidatePolicyDisabled;
  config.candidateNetworkPolicy = RTCCandidateNetworkPolicyLowCost;
  const int maxPackets = 60;
  const int timeout = 1500;
  const int interval = 2000;
  config.audioJitterBufferMaxPackets = maxPackets;
  config.audioJitterBufferFastAccelerate = YES;
  config.iceConnectionReceivingTimeout = timeout;
  config.iceBackupCandidatePairPingInterval = interval;
  config.continualGatheringPolicy =
      RTCContinualGatheringPolicyGatherContinually;
  config.shouldPruneTurnPorts = YES;
  config.activeResetSrtpParams = YES;
  config.cryptoOptions =
      [[RTC_OBJC_TYPE(RTCCryptoOptions) alloc] initWithSrtpEnableGcmCryptoSuites:YES
                                             srtpEnableAes128Sha1_32CryptoCipher:YES
                                          srtpEnableEncryptedRtpHeaderExtensions:NO
                                                    sframeRequireFrameEncryption:NO];

  RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                           optionalConstraints:nil];
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *factory =
      [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];

  RTC_OBJC_TYPE(RTCConfiguration) * newConfig;
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnection) *peerConnection =
        [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
    newConfig = peerConnection.configuration;

    XCTAssertTrue([peerConnection setBweMinBitrateBps:[NSNumber numberWithInt:100000]
                                    currentBitrateBps:[NSNumber numberWithInt:5000000]
                                        maxBitrateBps:[NSNumber numberWithInt:500000000]]);
    XCTAssertFalse([peerConnection setBweMinBitrateBps:[NSNumber numberWithInt:2]
                                     currentBitrateBps:[NSNumber numberWithInt:1]
                                         maxBitrateBps:nil]);
  }

  XCTAssertEqual([config.iceServers count], [newConfig.iceServers count]);
  RTC_OBJC_TYPE(RTCIceServer) *newServer = newConfig.iceServers[0];
  RTC_OBJC_TYPE(RTCIceServer) *origServer = config.iceServers[0];
  std::string origUrl = origServer.urlStrings.firstObject.UTF8String;
  std::string url = newServer.urlStrings.firstObject.UTF8String;
  XCTAssertEqual(origUrl, url);

  XCTAssertEqual(config.iceTransportPolicy, newConfig.iceTransportPolicy);
  XCTAssertEqual(config.bundlePolicy, newConfig.bundlePolicy);
  XCTAssertEqual(config.rtcpMuxPolicy, newConfig.rtcpMuxPolicy);
  XCTAssertEqual(config.tcpCandidatePolicy, newConfig.tcpCandidatePolicy);
  XCTAssertEqual(config.candidateNetworkPolicy, newConfig.candidateNetworkPolicy);
  XCTAssertEqual(config.audioJitterBufferMaxPackets, newConfig.audioJitterBufferMaxPackets);
  XCTAssertEqual(config.audioJitterBufferFastAccelerate, newConfig.audioJitterBufferFastAccelerate);
  XCTAssertEqual(config.iceConnectionReceivingTimeout, newConfig.iceConnectionReceivingTimeout);
  XCTAssertEqual(config.iceBackupCandidatePairPingInterval,
                 newConfig.iceBackupCandidatePairPingInterval);
  XCTAssertEqual(config.continualGatheringPolicy, newConfig.continualGatheringPolicy);
  XCTAssertEqual(config.shouldPruneTurnPorts, newConfig.shouldPruneTurnPorts);
  XCTAssertEqual(config.activeResetSrtpParams, newConfig.activeResetSrtpParams);
  XCTAssertEqual(config.cryptoOptions.srtpEnableGcmCryptoSuites,
                 newConfig.cryptoOptions.srtpEnableGcmCryptoSuites);
  XCTAssertEqual(config.cryptoOptions.srtpEnableAes128Sha1_32CryptoCipher,
                 newConfig.cryptoOptions.srtpEnableAes128Sha1_32CryptoCipher);
  XCTAssertEqual(config.cryptoOptions.srtpEnableEncryptedRtpHeaderExtensions,
                 newConfig.cryptoOptions.srtpEnableEncryptedRtpHeaderExtensions);
  XCTAssertEqual(config.cryptoOptions.sframeRequireFrameEncryption,
                 newConfig.cryptoOptions.sframeRequireFrameEncryption);
}

- (void)testWithDependencies {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTC_OBJC_TYPE(RTCIceServer) *server =
      [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithURLStrings:urlStrings];

  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
  config.iceServers = @[ server ];
  RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                           optionalConstraints:nil];
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *factory =
      [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];

  std::unique_ptr<webrtc::PeerConnectionDependencies> pc_dependencies =
      std::make_unique<webrtc::PeerConnectionDependencies>(nullptr);
  @autoreleasepool {
    RTC_OBJC_TYPE(RTCPeerConnection) *peerConnection =
        [factory peerConnectionWithDependencies:config
                                    constraints:contraints
                                   dependencies:std::move(pc_dependencies)
                                       delegate:nil];
    XCTAssertNotEqual(peerConnection, nil);
  }
}

- (void)testWithInvalidSDP {
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *factory =
      [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];

  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
  RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                           optionalConstraints:nil];
  RTC_OBJC_TYPE(RTCPeerConnection) *peerConnection =
      [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];

  dispatch_semaphore_t negotiatedSem = dispatch_semaphore_create(0);
  [peerConnection setRemoteDescription:[[RTC_OBJC_TYPE(RTCSessionDescription) alloc]
                                           initWithType:RTCSdpTypeOffer
                                                    sdp:@"invalid"]
                     completionHandler:^(NSError *error) {
                       XCTAssertNotEqual(error, nil);
                       if (error != nil) {
                         dispatch_semaphore_signal(negotiatedSem);
                       }
                     }];

  NSTimeInterval timeout = 5;
  XCTAssertEqual(
      0,
      dispatch_semaphore_wait(negotiatedSem,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC))));
  [peerConnection close];
}

- (void)testWithInvalidIceCandidate {
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *factory =
      [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];

  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
  RTC_OBJC_TYPE(RTCMediaConstraints) *contraints =
      [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc] initWithMandatoryConstraints:@{}
                                                           optionalConstraints:nil];
  RTC_OBJC_TYPE(RTCPeerConnection) *peerConnection =
      [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];

  dispatch_semaphore_t negotiatedSem = dispatch_semaphore_create(0);
  [peerConnection addIceCandidate:[[RTC_OBJC_TYPE(RTCIceCandidate) alloc] initWithSdp:@"invalid"
                                                                        sdpMLineIndex:-1
                                                                               sdpMid:nil]
                completionHandler:^(NSError *error) {
                  XCTAssertNotEqual(error, nil);
                  if (error != nil) {
                    dispatch_semaphore_signal(negotiatedSem);
                  }
                }];

  NSTimeInterval timeout = 5;
  XCTAssertEqual(
      0,
      dispatch_semaphore_wait(negotiatedSem,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC))));
  [peerConnection close];
}

@end
