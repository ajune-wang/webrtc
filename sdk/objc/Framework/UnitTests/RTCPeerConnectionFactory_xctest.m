/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <WebRTC/RTCAudioSource.h>
#import <WebRTC/RTCConfiguration.h>
#import <WebRTC/RTCDataChannel.h>
#import <WebRTC/RTCDataChannelConfiguration.h>
#import <WebRTC/RTCMediaConstraints.h>
#import <WebRTC/RTCMediaStreamTrack.h>
#import <WebRTC/RTCPeerConnection.h>
#import <WebRTC/RTCPeerConnectionFactory.h>
#import <WebRTC/RTCRtpReceiver.h>
#import <WebRTC/RTCRtpSender.h>
#import <WebRTC/RTCRtpTransceiver.h>
#import <WebRTC/RTCVideoSource.h>

#import <XCTest/XCTest.h>

@interface RTCPeerConnectionFactoryTests : XCTestCase
@end

@implementation RTCPeerConnectionFactoryTests

- (void)testPeerConnectionLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];

    RTCMediaConstraints *contraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    RTCPeerConnectionFactory *factory;
    RTCPeerConnection *peerConnection;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      [peerConnection close];
      factory = nil;
    }
    peerConnection = nil;
  }

  XCTAssertTrue(true, @"Expect test does not crash");
}

- (void)testMediaStreamLifetime {
  @autoreleasepool {
    RTCPeerConnectionFactory *factory;
    RTCMediaStream *mediaStream;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      mediaStream = [factory mediaStreamWithStreamId:@"mediaStream"];
      factory = nil;
    }
    mediaStream = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testDataChannelLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];
    RTCMediaConstraints *contraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];
    RTCDataChannelConfiguration *dataChannelConfig = [[RTCDataChannelConfiguration alloc] init];

    RTCPeerConnectionFactory *factory;
    RTCPeerConnection *peerConnection;
    RTCDataChannel *dataChannel;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      dataChannel =
          [peerConnection dataChannelForLabel:@"test_channel" configuration:dataChannelConfig];
      XCTAssertTrue(dataChannel != nil);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    dataChannel = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpTransceiverLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];
    config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
    RTCMediaConstraints *contraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];
    RTCRtpTransceiverInit *init = [[RTCRtpTransceiverInit alloc] init];

    RTCPeerConnectionFactory *factory;
    RTCPeerConnection *peerConnection;
    RTCRtpTransceiver *tranceiver;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      tranceiver = [peerConnection addTransceiverOfType:RTCRtpMediaTypeAudio init:init];
      XCTAssertTrue(tranceiver != nil);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    tranceiver = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpSenderLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];
    RTCMediaConstraints *contraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    RTCPeerConnectionFactory *factory;
    RTCPeerConnection *peerConnection;
    RTCRtpSender *sender;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      peerConnection =
          [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
      sender = [peerConnection senderWithKind:kRTCMediaStreamTrackKindVideo streamId:@"stream"];
      XCTAssertTrue(sender != nil);
      [peerConnection close];
      peerConnection = nil;
      factory = nil;
    }
    sender = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (void)testRTCRtpReceiverLifetime {
  @autoreleasepool {
    RTCConfiguration *config = [[RTCConfiguration alloc] init];
    RTCMediaConstraints *constraints =
        [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{} optionalConstraints:nil];

    RTCPeerConnectionFactory *factory;
    RTCPeerConnection *pc1;
    RTCPeerConnection *pc2;

    NSArray<RTCRtpReceiver *> *receivers1;
    NSArray<RTCRtpReceiver *> *receivers2;

    @autoreleasepool {
      factory = [[RTCPeerConnectionFactory alloc] init];
      pc1 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [pc1 senderWithKind:kRTCMediaStreamTrackKindAudio streamId:@"stream"];

      pc2 = [factory peerConnectionWithConfiguration:config constraints:constraints delegate:nil];
      [pc2 senderWithKind:kRTCMediaStreamTrackKindAudio streamId:@"stream"];

      NSTimeInterval negotiationTimeout = 15;
      XCTAssertTrue([self negotiatePeerConnection:pc1
                               withPeerConnection:pc2
                               negotiationTimeout:negotiationTimeout]);

      XCTAssertTrue(pc1.signalingState == RTCSignalingStateStable);
      XCTAssertTrue(pc2.signalingState == RTCSignalingStateStable);

      receivers1 = pc1.receivers;
      receivers2 = pc2.receivers;
      XCTAssertTrue(receivers1.count > 0);
      XCTAssertTrue(receivers2.count > 0);
      [pc1 close];
      [pc2 close];
      pc1 = nil;
      pc2 = nil;
      factory = nil;
    }
    receivers1 = nil;
    receivers2 = nil;
  }

  XCTAssertTrue(true, "Expect test does not crash");
}

- (bool)negotiatePeerConnection:(RTCPeerConnection *)pc1
             withPeerConnection:(RTCPeerConnection *)pc2
             negotiationTimeout:(NSTimeInterval)timeout {
  __weak RTCPeerConnection *weakPC1 = pc1;
  __weak RTCPeerConnection *weakPC2 = pc2;
  RTCMediaConstraints *sdpConstraints =
      [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{
        kRTCMediaConstraintsOfferToReceiveAudio : kRTCMediaConstraintsValueTrue
      }
                                            optionalConstraints:nil];

  dispatch_semaphore_t negotiatedSem = dispatch_semaphore_create(0);
  [weakPC1 offerForConstraints:sdpConstraints
             completionHandler:^(RTCSessionDescription *offer, NSError *error) {
               XCTAssertTrue(error == nil);
               XCTAssertTrue(offer != nil);
               [weakPC1
                   setLocalDescription:offer
                     completionHandler:^(NSError *error) {
                       XCTAssertTrue(error == nil);
                       [weakPC2
                           setRemoteDescription:offer
                              completionHandler:^(NSError *error) {
                                XCTAssertTrue(error == nil);
                                [weakPC2
                                    answerForConstraints:sdpConstraints
                                       completionHandler:^(RTCSessionDescription *answer,
                                                           NSError *error) {
                                         XCTAssertTrue(error == nil);
                                         XCTAssertTrue(answer != nil);
                                         [weakPC2
                                             setLocalDescription:answer
                                               completionHandler:^(NSError *error) {
                                                 XCTAssertTrue(error == nil);
                                                 [weakPC1
                                                     setRemoteDescription:answer
                                                        completionHandler:^(NSError *error) {
                                                          XCTAssertTrue(error == nil);
                                                          dispatch_semaphore_signal(negotiatedSem);
                                                        }];
                                               }];
                                       }];
                              }];
                     }];
             }];

  return 0 ==
      dispatch_semaphore_wait(
             negotiatedSem,
             dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * (1000 * 1000 * 1000))));
}

@end
