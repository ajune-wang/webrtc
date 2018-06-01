/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"

#include <string.h>  // memcmp

#include <memory>
#include <utility>
#include <vector>

#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"

namespace webrtc {

namespace test {

const RTPExtensionType kExtensionTypes[] = {
    RTPExtensionType::kRtpExtensionTransmissionTimeOffset,
    RTPExtensionType::kRtpExtensionAbsoluteSendTime,
    RTPExtensionType::kRtpExtensionTransportSequenceNumber,
    RTPExtensionType::kRtpExtensionAudioLevel,
    RTPExtensionType::kRtpExtensionVideoRotation};
const char* kExtensionNames[] = {
    RtpExtension::kTimestampOffsetUri, RtpExtension::kAbsSendTimeUri,
    RtpExtension::kTransportSequenceNumberUri, RtpExtension::kAudioLevelUri,
    RtpExtension::kVideoRotationUri};

const size_t kNumExtensions = 5;

std::unique_ptr<RtcEventAlrState> GenerateRtcEventAlrState(Random* prng) {
  return rtc::MakeUnique<RtcEventAlrState>(prng->Rand<bool>());
}

std::unique_ptr<RtcEventAudioPlayout> GenerateRtcEventAudioPlayout(
    uint32_t ssrc,
    Random* prng) {
  return rtc::MakeUnique<RtcEventAudioPlayout>(ssrc);
}

std::unique_ptr<RtcEventAudioNetworkAdaptation>
GenerateRtcEventAudioNetworkAdaptation(Random* prng) {
  std::unique_ptr<AudioEncoderRuntimeConfig> config =
      rtc::MakeUnique<AudioEncoderRuntimeConfig>();

  config->bitrate_bps = prng->Rand(0, 3000000);
  config->enable_fec = prng->Rand<bool>();
  config->enable_dtx = prng->Rand<bool>();
  config->frame_length_ms = prng->Rand(10, 120);
  config->num_channels = prng->Rand(1, 2);
  config->uplink_packet_loss_fraction = prng->Rand<float>();

  return rtc::MakeUnique<RtcEventAudioNetworkAdaptation>(std::move(config));
}

std::unique_ptr<RtcEventBweUpdateDelayBased>
GenerateRtcEventBweUpdateDelayBased(Random* prng) {
  constexpr int32_t kMaxBweBps = 20000000;
  constexpr size_t kNumBwStates = 3;
  BandwidthUsage states[kNumBwStates] = {BandwidthUsage::kBwNormal,
                                         BandwidthUsage::kBwUnderusing,
                                         BandwidthUsage::kBwOverusing};

  int32_t bitrate_bps = prng->Rand(0, kMaxBweBps);
  BandwidthUsage state = states[prng->Rand(0, kNumBwStates - 1)];
  return rtc::MakeUnique<RtcEventBweUpdateDelayBased>(bitrate_bps, state);
}

std::unique_ptr<RtcEventBweUpdateLossBased> GenerateRtcEventBweUpdateLossBased(
    Random* prng) {
  constexpr int32_t kMaxBweBps = 20000000;
  constexpr int32_t kMaxPackets = 1000;
  int32_t bitrate_bps = prng->Rand(0, kMaxBweBps);
  uint8_t fraction_lost = prng->Rand<uint8_t>();
  int32_t total_packets = prng->Rand(1, kMaxPackets);

  return rtc::MakeUnique<RtcEventBweUpdateLossBased>(bitrate_bps, fraction_lost,
                                                     total_packets);
}

std::unique_ptr<RtcEventProbeClusterCreated>
GenerateRtcEventProbeClusterCreated(Random* prng) {
  constexpr int kMaxBweBps = 20000000;
  constexpr int kMaxNumProbes = 10000;
  int id = prng->Rand(1, kMaxNumProbes);
  int bitrate_bps = prng->Rand(0, kMaxBweBps);
  int min_probes = prng->Rand(5, 50);
  int min_bytes = prng->Rand(500, 50000);

  return rtc::MakeUnique<RtcEventProbeClusterCreated>(id, bitrate_bps,
                                                      min_probes, min_bytes);
}

std::unique_ptr<RtcEventProbeResultFailure> GenerateRtcEventProbeResultFailure(
    Random* prng) {
  constexpr int kMaxNumProbes = 10000;
  constexpr size_t kNumFailureReasons = 3;
  ProbeFailureReason reasons[kNumFailureReasons] = {
      ProbeFailureReason::kInvalidSendReceiveInterval,
      ProbeFailureReason::kInvalidSendReceiveRatio,
      ProbeFailureReason::kTimeout};

  int id = prng->Rand(1, kMaxNumProbes);
  ProbeFailureReason reason = reasons[prng->Rand(0, kNumFailureReasons - 1)];

  return rtc::MakeUnique<RtcEventProbeResultFailure>(id, reason);
}

std::unique_ptr<RtcEventProbeResultSuccess> GenerateRtcEventProbeResultSuccess(
    Random* prng) {
  constexpr int kMaxBweBps = 20000000;
  constexpr int kMaxNumProbes = 10000;
  int id = prng->Rand(1, kMaxNumProbes);
  int bitrate_bps = prng->Rand(0, kMaxBweBps);

  return rtc::MakeUnique<RtcEventProbeResultSuccess>(id, bitrate_bps);
}

std::unique_ptr<RtcEventIceCandidatePairConfig>
GenerateRtcEventIceCandidatePairConfig(Random* prng) {
  constexpr size_t kNumIceConfigTypes = 4;
  IceCandidatePairConfigType event_types[kNumIceConfigTypes] = {
      IceCandidatePairConfigType::kAdded, IceCandidatePairConfigType::kUpdated,
      IceCandidatePairConfigType::kDestroyed,
      IceCandidatePairConfigType::kSelected};

  constexpr size_t kNumCandidateTypes = 5;
  IceCandidateType candidate_types[kNumCandidateTypes] = {
      IceCandidateType::kLocal, IceCandidateType::kStun,
      IceCandidateType::kPrflx, IceCandidateType::kRelay,
      IceCandidateType::kUnknown};

  constexpr size_t kNumProtocols = 5;
  IceCandidatePairProtocol protocol_types[kNumProtocols] = {
      IceCandidatePairProtocol::kUdp, IceCandidatePairProtocol::kTcp,
      IceCandidatePairProtocol::kSsltcp, IceCandidatePairProtocol::kTls,
      IceCandidatePairProtocol::kUnknown};

  constexpr size_t kNumAddressFamilies = 3;
  IceCandidatePairAddressFamily address_families[kNumAddressFamilies] = {
      IceCandidatePairAddressFamily::kIpv4,
      IceCandidatePairAddressFamily::kIpv6,
      IceCandidatePairAddressFamily::kUnknown};

  constexpr size_t kNumNetworkTypes = 6;
  IceCandidateNetworkType network_types[kNumNetworkTypes] = {
      IceCandidateNetworkType::kEthernet, IceCandidateNetworkType::kLoopback,
      IceCandidateNetworkType::kWifi,     IceCandidateNetworkType::kVpn,
      IceCandidateNetworkType::kCellular, IceCandidateNetworkType::kUnknown};

  IceCandidatePairConfigType type =
      event_types[prng->Rand(0, kNumIceConfigTypes - 1)];
  uint32_t pair_id = prng->Rand<uint32_t>();

  IceCandidatePairDescription desc;

  desc.local_candidate_type =
      candidate_types[prng->Rand(0, kNumCandidateTypes - 1)];
  desc.local_relay_protocol = protocol_types[prng->Rand(0, kNumProtocols - 1)];
  desc.local_network_type = network_types[prng->Rand(0, kNumNetworkTypes - 1)];
  desc.local_address_family =
      address_families[prng->Rand(0, kNumAddressFamilies - 1)];
  desc.remote_candidate_type =
      candidate_types[prng->Rand(0, kNumCandidateTypes - 1)];
  desc.remote_address_family =
      address_families[prng->Rand(0, kNumAddressFamilies - 1)];
  desc.candidate_pair_protocol =
      protocol_types[prng->Rand(0, kNumProtocols - 1)];

  return rtc::MakeUnique<RtcEventIceCandidatePairConfig>(type, pair_id, desc);
}

std::unique_ptr<RtcEventIceCandidatePair> GenerateRtcEventIceCandidatePair(
    Random* prng) {
  constexpr size_t kNumIceCheckTypes = 8;
  IceCandidatePairEventType event_types[kNumIceCheckTypes] = {
      IceCandidatePairEventType::kCheckSent,
      IceCandidatePairEventType::kCheckReceived,
      IceCandidatePairEventType::kCheckResponseSent,
      IceCandidatePairEventType::kCheckResponseReceived};
  IceCandidatePairEventType type =
      event_types[prng->Rand(0, kNumIceCheckTypes - 1)];
  uint32_t pair_id = prng->Rand<uint32_t>();

  return rtc::MakeUnique<RtcEventIceCandidatePair>(type, pair_id);
}

std::unique_ptr<RtcEventRtcpPacketIncoming> GenerateRtcEventRtcpPacketIncoming(
    Random* prng) {
  // TODO(terelius): Test the other RTCP types too.
  rtcp::ReportBlock report_block;
  report_block.SetMediaSsrc(prng->Rand<uint32_t>());  // Remote SSRC.
  report_block.SetFractionLost(prng->Rand(50));

  rtcp::SenderReport sender_report;
  sender_report.SetSenderSsrc(prng->Rand<uint32_t>());
  sender_report.SetNtp(NtpTime(prng->Rand<uint32_t>(), prng->Rand<uint32_t>()));
  sender_report.SetPacketCount(prng->Rand<uint32_t>());
  sender_report.AddReportBlock(report_block);

  rtc::Buffer buffer = sender_report.Build();
  return rtc::MakeUnique<RtcEventRtcpPacketIncoming>(buffer);
}

std::unique_ptr<RtcEventRtcpPacketOutgoing> GenerateRtcEventRtcpPacketOutgoing(
    Random* prng) {
  // TODO(terelius): Test the other RTCP types too.
  rtcp::ReportBlock report_block;
  report_block.SetMediaSsrc(prng->Rand<uint32_t>());  // Remote SSRC.
  report_block.SetFractionLost(prng->Rand(50));

  rtcp::SenderReport sender_report;
  sender_report.SetSenderSsrc(prng->Rand<uint32_t>());
  sender_report.SetNtp(NtpTime(prng->Rand<uint32_t>(), prng->Rand<uint32_t>()));
  sender_report.SetPacketCount(prng->Rand<uint32_t>());
  sender_report.AddReportBlock(report_block);

  rtc::Buffer buffer = sender_report.Build();
  return rtc::MakeUnique<RtcEventRtcpPacketOutgoing>(buffer);
}

std::unique_ptr<RtcEventRtpPacketIncoming> GenerateRtcEventRtpPacketIncoming(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map,
    Random* prng) {
  constexpr int kMaxCsrcs = 3;
  constexpr int kMaxNumExtensions = 5;
  size_t packet_size = prng->Rand(16 + 4 * kMaxCsrcs + 4 * kMaxNumExtensions,
                                  IP_PACKET_SIZE - 1);

  RtpPacketReceived rtp_packet(&extension_map);
  rtp_packet.SetPayloadType(prng->Rand(127));
  rtp_packet.SetMarker(prng->Rand<bool>());
  rtp_packet.SetSequenceNumber(prng->Rand<uint16_t>());
  rtp_packet.SetSsrc(ssrc);
  rtp_packet.SetTimestamp(prng->Rand<uint32_t>());

  uint32_t csrcs_count = prng->Rand(0, kMaxCsrcs);
  std::vector<uint32_t> csrcs;
  for (unsigned i = 0; i < csrcs_count; i++) {
    csrcs.push_back(prng->Rand<uint32_t>());
  }
  rtp_packet.SetCsrcs(csrcs);

  if (extension_map.IsRegistered(TransmissionOffset::kId))
    rtp_packet.SetExtension<TransmissionOffset>(prng->Rand(0x00ffffff));
  if (extension_map.IsRegistered(AudioLevel::kId))
    rtp_packet.SetExtension<AudioLevel>(prng->Rand<bool>(), prng->Rand(127));
  if (extension_map.IsRegistered(AbsoluteSendTime::kId))
    rtp_packet.SetExtension<AbsoluteSendTime>(prng->Rand(0x00ffffff));
  if (extension_map.IsRegistered(VideoOrientation::kId))
    rtp_packet.SetExtension<VideoOrientation>(prng->Rand(2));
  if (extension_map.IsRegistered(TransportSequenceNumber::kId))
    rtp_packet.SetExtension<TransportSequenceNumber>(prng->Rand<uint16_t>());

  RTC_DCHECK_GE(packet_size, rtp_packet.headers_size());
  size_t payload_size = packet_size - rtp_packet.headers_size();
  RTC_CHECK_LE(rtp_packet.headers_size() + payload_size, IP_PACKET_SIZE);
  uint8_t* payload = rtp_packet.AllocatePayload(payload_size);
  for (size_t i = 0; i < payload_size; i++) {
    payload[i] = prng->Rand<uint8_t>();
  }

  return rtc::MakeUnique<RtcEventRtpPacketIncoming>(rtp_packet);
}

std::unique_ptr<RtcEventRtpPacketOutgoing> GenerateRtcEventRtpPacketOutgoing(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extension_map,
    Random* prng) {
  constexpr int kMaxCsrcs = 3;
  constexpr int kMaxNumExtensions = 5;
  size_t packet_size = prng->Rand(16 + 4 * kMaxCsrcs + 4 * kMaxNumExtensions,
                                  IP_PACKET_SIZE - 1);

  RtpPacketToSend rtp_packet(&extension_map, packet_size);
  rtp_packet.SetPayloadType(prng->Rand(127));
  rtp_packet.SetMarker(prng->Rand<bool>());
  rtp_packet.SetSequenceNumber(prng->Rand<uint16_t>());
  rtp_packet.SetSsrc(ssrc);
  rtp_packet.SetTimestamp(prng->Rand<uint32_t>());

  uint32_t csrcs_count = prng->Rand(0, kMaxCsrcs);
  std::vector<uint32_t> csrcs;
  for (unsigned i = 0; i < csrcs_count; i++) {
    csrcs.push_back(prng->Rand<uint32_t>());
  }
  rtp_packet.SetCsrcs(csrcs);

  if (extension_map.IsRegistered(TransmissionOffset::kId))
    rtp_packet.SetExtension<TransmissionOffset>(prng->Rand(0x00ffffff));
  if (extension_map.IsRegistered(AudioLevel::kId))
    rtp_packet.SetExtension<AudioLevel>(prng->Rand<bool>(), prng->Rand(127));
  if (extension_map.IsRegistered(AbsoluteSendTime::kId))
    rtp_packet.SetExtension<AbsoluteSendTime>(prng->Rand(0x00ffffff));
  if (extension_map.IsRegistered(VideoOrientation::kId))
    rtp_packet.SetExtension<VideoOrientation>(prng->Rand(2));
  if (extension_map.IsRegistered(TransportSequenceNumber::kId))
    rtp_packet.SetExtension<TransportSequenceNumber>(prng->Rand<uint16_t>());

  RTC_DCHECK_GE(packet_size, rtp_packet.headers_size());
  size_t payload_size = packet_size - rtp_packet.headers_size();
  RTC_CHECK_LE(rtp_packet.headers_size() + payload_size, IP_PACKET_SIZE);
  uint8_t* payload = rtp_packet.AllocatePayload(payload_size);
  for (size_t i = 0; i < payload_size; i++) {
    payload[i] = prng->Rand<uint8_t>();
  }

  int probe_cluster_id = prng->Rand(0, 100000);
  return rtc::MakeUnique<RtcEventRtpPacketOutgoing>(rtp_packet,
                                                    probe_cluster_id);
}

RtpHeaderExtensionMap GenerateRtpHeaderExtensionMap(Random* prng) {
  RtpHeaderExtensionMap extension_map;
  if (prng->Rand<bool>()) {
    extension_map.Register<AudioLevel>(prng->Rand(1, 2));
  }
  if (prng->Rand<bool>()) {
    extension_map.Register<TransmissionOffset>(prng->Rand(3, 4));
  }
  if (prng->Rand<bool>()) {
    extension_map.Register<AbsoluteSendTime>(prng->Rand(5, 6));
  }
  if (prng->Rand<bool>()) {
    extension_map.Register<VideoOrientation>(prng->Rand(7, 8));
  }
  if (prng->Rand<bool>()) {
    extension_map.Register<TransportSequenceNumber>(prng->Rand(9, 10));
  }

  return extension_map;
}

std::unique_ptr<RtcEventAudioReceiveStreamConfig>
GenerateRtcEventAudioReceiveStreamConfig(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extensions,
    Random* prng) {
  auto config = rtc::MakeUnique<rtclog::StreamConfig>();
  // Add SSRCs for the stream.
  config->remote_ssrc = ssrc;
  config->local_ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }

  return rtc::MakeUnique<RtcEventAudioReceiveStreamConfig>(std::move(config));
}

std::unique_ptr<RtcEventAudioSendStreamConfig>
GenerateRtcEventAudioSendStreamConfig(uint32_t ssrc,
                                      const RtpHeaderExtensionMap& extensions,
                                      Random* prng) {
  auto config = rtc::MakeUnique<rtclog::StreamConfig>();
  // Add SSRC to the stream.
  config->local_ssrc = ssrc;
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
  return rtc::MakeUnique<RtcEventAudioSendStreamConfig>(std::move(config));
}

std::unique_ptr<RtcEventVideoReceiveStreamConfig>
GenerateRtcEventVideoReceiveStreamConfig(
    uint32_t ssrc,
    const RtpHeaderExtensionMap& extensions,
    Random* prng) {
  auto config = rtc::MakeUnique<rtclog::StreamConfig>();

  // Add SSRCs for the stream.
  config->remote_ssrc = ssrc;
  config->local_ssrc = prng->Rand<uint32_t>();
  // Add extensions and settings for RTCP.
  config->rtcp_mode =
      prng->Rand<bool>() ? RtcpMode::kCompound : RtcpMode::kReducedSize;
  config->remb = prng->Rand<bool>();
  config->rtx_ssrc = prng->Rand<uint32_t>();
  config->codecs.emplace_back(prng->Rand<bool>() ? "VP8" : "H264",
                              prng->Rand(1, 127), prng->Rand(1, 127));
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
  return rtc::MakeUnique<RtcEventVideoReceiveStreamConfig>(std::move(config));
}

std::unique_ptr<RtcEventVideoSendStreamConfig>
GenerateRtcEventVideoSendStreamConfig(uint32_t ssrc,
                                      const RtpHeaderExtensionMap& extensions,
                                      Random* prng) {
  auto config = rtc::MakeUnique<rtclog::StreamConfig>();

  config->codecs.emplace_back(prng->Rand<bool>() ? "VP8" : "H264",
                              prng->Rand(1, 127), prng->Rand(1, 127));
  config->local_ssrc = ssrc;
  config->rtx_ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
  return rtc::MakeUnique<RtcEventVideoSendStreamConfig>(std::move(config));
}

bool VerifyLoggedAlrStateEvent(const RtcEventAlrState& original_event,
                               const LoggedAlrStateEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.in_alr_ != logged_event.in_alr)
    return false;
  return true;
}

bool VerifyLoggedAudioPlayoutEvent(
    const RtcEventAudioPlayout& original_event,
    const LoggedAudioPlayoutEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.ssrc_ != logged_event.ssrc)
    return false;
  return true;
}

bool VerifyLoggedAudioNetworkAdaptationEvent(
    const RtcEventAudioNetworkAdaptation& original_event,
    const LoggedAudioNetworkAdaptationEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.config_->bitrate_bps != logged_event.config.bitrate_bps)
    return false;
  if (original_event.config_->enable_dtx != logged_event.config.enable_dtx)
    return false;
  if (original_event.config_->enable_fec != logged_event.config.enable_fec)
    return false;
  if (original_event.config_->frame_length_ms !=
      logged_event.config.frame_length_ms)
    return false;
  if (original_event.config_->num_channels != logged_event.config.num_channels)
    return false;
  if (original_event.config_->uplink_packet_loss_fraction !=
      logged_event.config.uplink_packet_loss_fraction)
    return false;

  return true;
}

bool VerifyLoggedBweDelayBasedUpdate(
    const RtcEventBweUpdateDelayBased& original_event,
    const LoggedBweDelayBasedUpdate& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  if (original_event.detector_state_ != logged_event.detector_state)
    return false;
  return true;
}

bool VerifyLoggedBweLossBasedUpdate(
    const RtcEventBweUpdateLossBased& original_event,
    const LoggedBweLossBasedUpdate& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  if (original_event.fraction_loss_ != logged_event.fraction_lost)
    return false;
  if (original_event.total_packets_ != logged_event.expected_packets)
    return false;
  return true;
}

bool VerifyLoggedBweProbeClusterCreatedEvent(
    const RtcEventProbeClusterCreated& original_event,
    const LoggedBweProbeClusterCreatedEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.id_ != logged_event.id)
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  if (original_event.min_probes_ != logged_event.min_packets)
    return false;
  if (original_event.min_bytes_ != logged_event.min_bytes)
    return false;

  return true;
}

bool VerifyLoggedBweProbeFailureEvent(
    const RtcEventProbeResultFailure& original_event,
    const LoggedBweProbeFailureEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.id_ != logged_event.id)
    return false;
  if (original_event.failure_reason_ != logged_event.failure_reason)
    return false;
  return true;
}

bool VerifyLoggedBweProbeSuccessEvent(
    const RtcEventProbeResultSuccess& original_event,
    const LoggedBweProbeSuccessEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (original_event.id_ != logged_event.id)
    return false;
  if (original_event.bitrate_bps_ != logged_event.bitrate_bps)
    return false;
  return true;
}

bool VerifyLoggedIceCandidatePairConfig(
    const RtcEventIceCandidatePairConfig& original_event,
    const LoggedIceCandidatePairConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.type_ != logged_event.type)
    return false;
  if (original_event.candidate_pair_id_ != logged_event.candidate_pair_id)
    return false;
  if (original_event.candidate_pair_desc_.local_candidate_type !=
      logged_event.local_candidate_type)
    return false;
  if (original_event.candidate_pair_desc_.local_relay_protocol !=
      logged_event.local_relay_protocol)
    return false;
  if (original_event.candidate_pair_desc_.local_network_type !=
      logged_event.local_network_type)
    return false;
  if (original_event.candidate_pair_desc_.local_address_family !=
      logged_event.local_address_family)
    return false;
  if (original_event.candidate_pair_desc_.remote_candidate_type !=
      logged_event.remote_candidate_type)
    return false;
  if (original_event.candidate_pair_desc_.remote_address_family !=
      logged_event.remote_address_family)
    return false;
  if (original_event.candidate_pair_desc_.candidate_pair_protocol !=
      logged_event.candidate_pair_protocol)
    return false;

  return true;
}

bool VerifyLoggedIceCandidatePairEvent(
    const RtcEventIceCandidatePair& original_event,
    const LoggedIceCandidatePairEvent& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.type_ != logged_event.type)
    return false;
  if (original_event.candidate_pair_id_ != logged_event.candidate_pair_id)
    return false;

  return true;
}

bool VerifyLoggedRtpHeader(const RtpPacket& original_header,
                           const RTPHeader& logged_header) {
  // Standard RTP header.
  if (original_header.Marker() != logged_header.markerBit)
    return false;
  if (original_header.PayloadType() != logged_header.payloadType)
    return false;
  if (original_header.SequenceNumber() != logged_header.sequenceNumber)
    return false;
  if (original_header.Timestamp() != logged_header.timestamp)
    return false;
  if (original_header.Ssrc() != logged_header.ssrc)
    return false;
  if (original_header.Csrcs().size() != logged_header.numCSRCs)
    return false;
  for (size_t i = 0; i < logged_header.numCSRCs; i++) {
    if (original_header.Csrcs()[i] != logged_header.arrOfCSRCs[i])
      return false;
  }
  if (original_header.padding_size() != logged_header.paddingLength)
    return false;
  if (original_header.headers_size() != logged_header.headerLength)
    return false;

  // TransmissionOffset header extension.
  if (original_header.HasExtension<TransmissionOffset>() !=
      logged_header.extension.hasTransmissionTimeOffset)
    return false;
  int32_t offset;
  original_header.GetExtension<TransmissionOffset>(&offset);
  if (offset != logged_header.extension.transmissionTimeOffset)
    return false;

  // AbsoluteSendTime header extension.
  if (original_header.HasExtension<AbsoluteSendTime>() !=
      logged_header.extension.hasAbsoluteSendTime)
    return false;
  uint32_t sendtime;
  original_header.GetExtension<AbsoluteSendTime>(&sendtime);
  if (sendtime != logged_header.extension.absoluteSendTime)
    return false;

  // TransportSequenceNumber header extension.
  if (original_header.HasExtension<TransportSequenceNumber>() !=
      logged_header.extension.hasTransportSequenceNumber)
    return false;
  uint16_t seqnum;
  original_header.GetExtension<TransportSequenceNumber>(&seqnum);
  if (seqnum != logged_header.extension.transportSequenceNumber)
    return false;

  // AudioLevel header extension.
  if (original_header.HasExtension<AudioLevel>() !=
      logged_header.extension.hasAudioLevel)
    return false;
  bool voice_activity;
  uint8_t audio_level;
  original_header.GetExtension<AudioLevel>(&voice_activity, &audio_level);
  if (voice_activity != logged_header.extension.voiceActivity)
    return false;
  if (audio_level != logged_header.extension.audioLevel)
    return false;

  // VideoOrientation header extension.
  if (original_header.HasExtension<VideoOrientation>() !=
      logged_header.extension.hasVideoRotation)
    return false;
  uint8_t rotation;
  original_header.GetExtension<VideoOrientation>(&rotation);
  if (rotation != logged_header.extension.videoRotation)
    return false;

  return true;
}

bool VerifyLoggedRtpPacketIncoming(
    const RtcEventRtpPacketIncoming& original_event,
    const LoggedRtpPacketIncoming& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.header_.headers_size() != logged_event.rtp.header_length)
    return false;

  if (original_event.packet_length_ != logged_event.rtp.total_length)
    return false;

  if (!VerifyLoggedRtpHeader(original_event.header_, logged_event.rtp.header))
    return false;

  return true;
}

bool VerifyLoggedRtpPacketOutgoing(
    const RtcEventRtpPacketOutgoing& original_event,
    const LoggedRtpPacketOutgoing& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.header_.headers_size() != logged_event.rtp.header_length)
    return false;

  if (original_event.packet_length_ != logged_event.rtp.total_length)
    return false;

  if (!VerifyLoggedRtpHeader(original_event.header_, logged_event.rtp.header))
    return false;

  return true;
}

bool VerifyLoggedRtcpPacketIncoming(
    const RtcEventRtcpPacketIncoming& original_event,
    const LoggedRtcpPacketIncoming& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.packet_.size() != logged_event.rtcp.raw_data.size())
    return false;
  if (memcmp(original_event.packet_.data(), logged_event.rtcp.raw_data.data(),
             original_event.packet_.size()) != 0) {
    return false;
  }
  return true;
}

bool VerifyLoggedRtcpPacketOutgoing(
    const RtcEventRtcpPacketOutgoing& original_event,
    const LoggedRtcpPacketOutgoing& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;

  if (original_event.packet_.size() != logged_event.rtcp.raw_data.size())
    return false;
  if (memcmp(original_event.packet_.data(), logged_event.rtcp.raw_data.data(),
             original_event.packet_.size()) != 0) {
    return false;
  }
  return true;
}

bool VerifyLoggedStartEvent(int64_t start_time_us,
                            const LoggedStartEvent& logged_event) {
  if (start_time_us != logged_event.log_time_us())
    return false;
  return true;
}

bool VerifyLoggedStopEvent(int64_t stop_time_us,
                           const LoggedStopEvent& logged_event) {
  if (stop_time_us != logged_event.log_time_us())
    return false;
  return true;
}

bool VerifyLoggedAudioRecvConfig(
    const RtcEventAudioReceiveStreamConfig& original_event,
    const LoggedAudioRecvConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  return *original_event.config_ == logged_event.config;
}

bool VerifyLoggedAudioSendConfig(
    const RtcEventAudioSendStreamConfig& original_event,
    const LoggedAudioSendConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  return *original_event.config_ == logged_event.config;
}

bool VerifyLoggedVideoRecvConfig(
    const RtcEventVideoReceiveStreamConfig& original_event,
    const LoggedVideoRecvConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  return *original_event.config_ == logged_event.config;
}

bool VerifyLoggedVideoSendConfig(
    const RtcEventVideoSendStreamConfig& original_event,
    const LoggedVideoSendConfig& logged_event) {
  if (original_event.timestamp_us_ != logged_event.log_time_us())
    return false;
  if (logged_event.configs.size() == 1)
    return false;
  return *original_event.config_ == logged_event.configs[0];
}

}  // namespace test
}  // namespace webrtc
