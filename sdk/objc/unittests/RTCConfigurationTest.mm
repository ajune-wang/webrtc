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

#include <vector>

#import "api/peerconnection/RTCConfiguration+Private.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCIceServer.h"
#import "helpers/NSString+StdString.h"

@interface RTCConfigurationTest : XCTestCase
@end

@implementation RTCConfigurationTest

- (void)testConversionToNativeConfiguration {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTC_OBJC_TYPE(RTCIceServer) *server =
      [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithURLStrings:urlStrings];

  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  config.iceServers = @[ server ];
  config.iceTransportPolicy = RTCIceTransportPolicyRelay;
  config.bundlePolicy = RTCBundlePolicyMaxBundle;
  config.rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
  config.tcpCandidatePolicy = RTCTcpCandidatePolicyDisabled;
  config.candidateNetworkPolicy = RTCCandidateNetworkPolicyLowCost;
  const int maxPackets = 60;
  const int timeout = 1;
  const int interval = 2;
  config.audioJitterBufferMaxPackets = maxPackets;
  config.audioJitterBufferFastAccelerate = YES;
  config.iceConnectionReceivingTimeout = timeout;
  config.iceBackupCandidatePairPingInterval = interval;
  config.continualGatheringPolicy =
      RTCContinualGatheringPolicyGatherContinually;
  config.shouldPruneTurnPorts = YES;
  config.cryptoOptions =
      [[RTC_OBJC_TYPE(RTCCryptoOptions) alloc] initWithSrtpEnableGcmCryptoSuites:YES
                                             srtpEnableAes128Sha1_32CryptoCipher:YES
                                          srtpEnableEncryptedRtpHeaderExtensions:YES
                                                    sframeRequireFrameEncryption:YES];
  config.rtcpAudioReportIntervalMs = 2500;
  config.rtcpVideoReportIntervalMs = 3750;

  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration>
      nativeConfig([config createNativeConfiguration]);
  XCTAssertTrue(nativeConfig.get());
  XCTAssertEqual(1u, nativeConfig->servers.size());
  webrtc::PeerConnectionInterface::IceServer nativeServer =
      nativeConfig->servers.front();
  XCTAssertEqual(1u, nativeServer.urls.size());
  XCTAssertEqual("stun:stun1.example.net", nativeServer.urls.front());

  XCTAssertEqual(webrtc::PeerConnectionInterface::kRelay, nativeConfig->type);
  XCTAssertEqual(webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle,
                 nativeConfig->bundle_policy);
  XCTAssertEqual(webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate,
                 nativeConfig->rtcp_mux_policy);
  XCTAssertEqual(webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled,
                 nativeConfig->tcp_candidate_policy);
  XCTAssertEqual(webrtc::PeerConnectionInterface::kCandidateNetworkPolicyLowCost,
                 nativeConfig->candidate_network_policy);
  XCTAssertEqual(maxPackets, nativeConfig->audio_jitter_buffer_max_packets);
  XCTAssertEqual(true, nativeConfig->audio_jitter_buffer_fast_accelerate);
  XCTAssertEqual(timeout, nativeConfig->ice_connection_receiving_timeout);
  XCTAssertEqual(interval, nativeConfig->ice_backup_candidate_pair_ping_interval);
  XCTAssertEqual(webrtc::PeerConnectionInterface::GATHER_CONTINUALLY,
                 nativeConfig->continual_gathering_policy);
  XCTAssertEqual(true, nativeConfig->prune_turn_ports);
  XCTAssertEqual(true, nativeConfig->crypto_options->srtp.enable_gcm_crypto_suites);
  XCTAssertEqual(true, nativeConfig->crypto_options->srtp.enable_aes128_sha1_32_crypto_cipher);
  XCTAssertEqual(true, nativeConfig->crypto_options->srtp.enable_encrypted_rtp_header_extensions);
  XCTAssertEqual(true, nativeConfig->crypto_options->sframe.require_frame_encryption);
  XCTAssertEqual(2500, nativeConfig->audio_rtcp_report_interval_ms());
  XCTAssertEqual(3750, nativeConfig->video_rtcp_report_interval_ms());
}

- (void)testNativeConversionToConfiguration {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTC_OBJC_TYPE(RTCIceServer) *server =
      [[RTC_OBJC_TYPE(RTCIceServer) alloc] initWithURLStrings:urlStrings];

  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  config.iceServers = @[ server ];
  config.iceTransportPolicy = RTCIceTransportPolicyRelay;
  config.bundlePolicy = RTCBundlePolicyMaxBundle;
  config.rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
  config.tcpCandidatePolicy = RTCTcpCandidatePolicyDisabled;
  config.candidateNetworkPolicy = RTCCandidateNetworkPolicyLowCost;
  const int maxPackets = 60;
  const int timeout = 1;
  const int interval = 2;
  config.audioJitterBufferMaxPackets = maxPackets;
  config.audioJitterBufferFastAccelerate = YES;
  config.iceConnectionReceivingTimeout = timeout;
  config.iceBackupCandidatePairPingInterval = interval;
  config.continualGatheringPolicy =
      RTCContinualGatheringPolicyGatherContinually;
  config.shouldPruneTurnPorts = YES;
  config.cryptoOptions =
      [[RTC_OBJC_TYPE(RTCCryptoOptions) alloc] initWithSrtpEnableGcmCryptoSuites:YES
                                             srtpEnableAes128Sha1_32CryptoCipher:NO
                                          srtpEnableEncryptedRtpHeaderExtensions:NO
                                                    sframeRequireFrameEncryption:NO];
  config.rtcpAudioReportIntervalMs = 1500;
  config.rtcpVideoReportIntervalMs = 2150;

  webrtc::PeerConnectionInterface::RTCConfiguration *nativeConfig =
      [config createNativeConfiguration];
  RTC_OBJC_TYPE(RTCConfiguration) *newConfig =
      [[RTC_OBJC_TYPE(RTCConfiguration) alloc] initWithNativeConfiguration:*nativeConfig];
  XCTAssertEqual([config.iceServers count], newConfig.iceServers.count);
  RTC_OBJC_TYPE(RTCIceServer) *newServer = newConfig.iceServers[0];
  RTC_OBJC_TYPE(RTCIceServer) *origServer = config.iceServers[0];
  XCTAssertEqual(origServer.urlStrings.count, server.urlStrings.count);
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
  XCTAssertEqual(config.cryptoOptions.srtpEnableGcmCryptoSuites,
                 newConfig.cryptoOptions.srtpEnableGcmCryptoSuites);
  XCTAssertEqual(config.cryptoOptions.srtpEnableAes128Sha1_32CryptoCipher,
                 newConfig.cryptoOptions.srtpEnableAes128Sha1_32CryptoCipher);
  XCTAssertEqual(config.cryptoOptions.srtpEnableEncryptedRtpHeaderExtensions,
                 newConfig.cryptoOptions.srtpEnableEncryptedRtpHeaderExtensions);
  XCTAssertEqual(config.cryptoOptions.sframeRequireFrameEncryption,
                 newConfig.cryptoOptions.sframeRequireFrameEncryption);
  XCTAssertEqual(config.rtcpAudioReportIntervalMs, newConfig.rtcpAudioReportIntervalMs);
  XCTAssertEqual(config.rtcpVideoReportIntervalMs, newConfig.rtcpVideoReportIntervalMs);
}

- (void)testDefaultValues {
  RTC_OBJC_TYPE(RTCConfiguration) *config = [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
  XCTAssertEqual(config.cryptoOptions, nil);
}

@end
