/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtcstats_objects.h"

namespace webrtc {

const char* const RTCDataChannelState::kConnecting = "connecting";
const char* const RTCDataChannelState::kOpen = "open";
const char* const RTCDataChannelState::kClosing = "closing";
const char* const RTCDataChannelState::kClosed = "closed";

const char* const RTCStatsIceCandidatePairState::kFrozen = "frozen";
const char* const RTCStatsIceCandidatePairState::kWaiting = "waiting";
const char* const RTCStatsIceCandidatePairState::kInProgress = "in-progress";
const char* const RTCStatsIceCandidatePairState::kFailed = "failed";
const char* const RTCStatsIceCandidatePairState::kSucceeded = "succeeded";

// Strings defined in https://tools.ietf.org/html/rfc5245.
const char* const RTCIceCandidateType::kHost = "host";
const char* const RTCIceCandidateType::kSrflx = "srflx";
const char* const RTCIceCandidateType::kPrflx = "prflx";
const char* const RTCIceCandidateType::kRelay = "relay";

const char* const RTCDtlsTransportState::kNew = "new";
const char* const RTCDtlsTransportState::kConnecting = "connecting";
const char* const RTCDtlsTransportState::kConnected = "connected";
const char* const RTCDtlsTransportState::kClosed = "closed";
const char* const RTCDtlsTransportState::kFailed = "failed";

const char* const RTCMediaStreamTrackKind::kAudio = "audio";
const char* const RTCMediaStreamTrackKind::kVideo = "video";

// https://w3c.github.io/webrtc-stats/#dom-rtcnetworktype
const char* const RTCNetworkType::kBluetooth = "bluetooth";
const char* const RTCNetworkType::kCellular = "cellular";
const char* const RTCNetworkType::kEthernet = "ethernet";
const char* const RTCNetworkType::kWifi = "wifi";
const char* const RTCNetworkType::kWimax = "wimax";
const char* const RTCNetworkType::kVpn = "vpn";
const char* const RTCNetworkType::kUnknown = "unknown";

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCCertificateStats, RTCStats, "certificate",
    &fingerprint,
    &fingerprint_algorithm,
    &base64_certificate,
    &issuer_certificate_id);
// clang-format on

RTCCertificateStats::RTCCertificateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCCertificateStats(std::string(id), timestamp_us) {
}

RTCCertificateStats::RTCCertificateStats(std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      fingerprint("fingerprint", true),
      fingerprint_algorithm("fingerprintAlgorithm", true),
      base64_certificate("base64Certificate", true),
      issuer_certificate_id("issuerCertificateId", true) {}

RTCCertificateStats::RTCCertificateStats(
    const RTCCertificateStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      fingerprint(other.fingerprint),
      fingerprint_algorithm(other.fingerprint_algorithm),
      base64_certificate(other.base64_certificate),
      issuer_certificate_id(other.issuer_certificate_id) {
}

RTCCertificateStats::~RTCCertificateStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCCodecStats, RTCStats, "codec",
    &payload_type,
    &mime_type,
    &clock_rate,
    &channels,
    &sdp_fmtp_line,
    &implementation);
// clang-format on

RTCCodecStats::RTCCodecStats(
    const std::string& id, int64_t timestamp_us)
    : RTCCodecStats(std::string(id), timestamp_us) {
}

RTCCodecStats::RTCCodecStats(std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      payload_type("payloadType", true),
      mime_type("mimeType", true),
      clock_rate("clockRate", true),
      channels("channels", true),
      sdp_fmtp_line("sdpFmtpLine", true),
      implementation("implementation", true) {}

RTCCodecStats::RTCCodecStats(
    const RTCCodecStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      payload_type(other.payload_type),
      mime_type(other.mime_type),
      clock_rate(other.clock_rate),
      channels(other.channels),
      sdp_fmtp_line(other.sdp_fmtp_line),
      implementation(other.implementation) {
}

RTCCodecStats::~RTCCodecStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCDataChannelStats, RTCStats, "data-channel",
    &label,
    &protocol,
    &datachannelid,
    &state,
    &messages_sent,
    &bytes_sent,
    &messages_received,
    &bytes_received);
// clang-format on

RTCDataChannelStats::RTCDataChannelStats(
    const std::string& id, int64_t timestamp_us)
    : RTCDataChannelStats(std::string(id), timestamp_us) {
}

RTCDataChannelStats::RTCDataChannelStats(std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      label("label", true),
      protocol("protocol", true),
      datachannelid("datachannelid", true),
      state("state", true),
      messages_sent("messagesSent", true),
      bytes_sent("bytesSent", true),
      messages_received("messagesReceived", true),
      bytes_received("bytesReceived", true) {}

RTCDataChannelStats::RTCDataChannelStats(
    const RTCDataChannelStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      label(other.label),
      protocol(other.protocol),
      datachannelid(other.datachannelid),
      state(other.state),
      messages_sent(other.messages_sent),
      bytes_sent(other.bytes_sent),
      messages_received(other.messages_received),
      bytes_received(other.bytes_received) {
}

RTCDataChannelStats::~RTCDataChannelStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCIceCandidatePairStats, RTCStats, "candidate-pair",
    &transport_id,
    &local_candidate_id,
    &remote_candidate_id,
    &state,
    &priority,
    &nominated,
    &writable,
    &readable,
    &bytes_sent,
    &bytes_received,
    &total_round_trip_time,
    &current_round_trip_time,
    &available_outgoing_bitrate,
    &available_incoming_bitrate,
    &requests_received,
    &requests_sent,
    &responses_received,
    &responses_sent,
    &retransmissions_received,
    &retransmissions_sent,
    &consent_requests_received,
    &consent_requests_sent,
    &consent_responses_received,
    &consent_responses_sent);
// clang-format on

RTCIceCandidatePairStats::RTCIceCandidatePairStats(
    const std::string& id, int64_t timestamp_us)
    : RTCIceCandidatePairStats(std::string(id), timestamp_us) {
}

RTCIceCandidatePairStats::RTCIceCandidatePairStats(std::string&& id,
                                                   int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      transport_id("transportId", true),
      local_candidate_id("localCandidateId", true),
      remote_candidate_id("remoteCandidateId", true),
      state("state", true),
      priority("priority", true),
      nominated("nominated", true),
      writable("writable", true),
      readable("readable", true),
      bytes_sent("bytesSent", true),
      bytes_received("bytesReceived", true),
      total_round_trip_time("totalRoundTripTime", true),
      current_round_trip_time("currentRoundTripTime", true),
      available_outgoing_bitrate("availableOutgoingBitrate", true),
      available_incoming_bitrate("availableIncomingBitrate", true),
      requests_received("requestsReceived", true),
      requests_sent("requestsSent", true),
      responses_received("responsesReceived", true),
      responses_sent("responsesSent", true),
      retransmissions_received("retransmissionsReceived", true),
      retransmissions_sent("retransmissionsSent", true),
      consent_requests_received("consentRequestsReceived", true),
      consent_requests_sent("consentRequestsSent", true),
      consent_responses_received("consentResponsesReceived", true),
      consent_responses_sent("consentResponsesSent", true) {}

RTCIceCandidatePairStats::RTCIceCandidatePairStats(
    const RTCIceCandidatePairStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      transport_id(other.transport_id),
      local_candidate_id(other.local_candidate_id),
      remote_candidate_id(other.remote_candidate_id),
      state(other.state),
      priority(other.priority),
      nominated(other.nominated),
      writable(other.writable),
      readable(other.readable),
      bytes_sent(other.bytes_sent),
      bytes_received(other.bytes_received),
      total_round_trip_time(other.total_round_trip_time),
      current_round_trip_time(other.current_round_trip_time),
      available_outgoing_bitrate(other.available_outgoing_bitrate),
      available_incoming_bitrate(other.available_incoming_bitrate),
      requests_received(other.requests_received),
      requests_sent(other.requests_sent),
      responses_received(other.responses_received),
      responses_sent(other.responses_sent),
      retransmissions_received(other.retransmissions_received),
      retransmissions_sent(other.retransmissions_sent),
      consent_requests_received(other.consent_requests_received),
      consent_requests_sent(other.consent_requests_sent),
      consent_responses_received(other.consent_responses_received),
      consent_responses_sent(other.consent_responses_sent) {
}

RTCIceCandidatePairStats::~RTCIceCandidatePairStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCIceCandidateStats, RTCStats, "abstract-ice-candidate",
    &transport_id,
    &is_remote,
    &network_type,
    &ip,
    &port,
    &protocol,
    &candidate_type,
    &priority,
    &url,
    &deleted);
// clang-format on

RTCIceCandidateStats::RTCIceCandidateStats(
    const std::string& id, int64_t timestamp_us, bool is_remote)
    : RTCIceCandidateStats(std::string(id), timestamp_us, is_remote) {
}

RTCIceCandidateStats::RTCIceCandidateStats(std::string&& id,
                                           int64_t timestamp_us,
                                           bool is_remote)
    : RTCStats(std::move(id), timestamp_us),
      transport_id("transportId", true),
      is_remote("isRemote", is_remote, true),
      network_type("networkType", true),
      ip("ip", true),
      port("port", true),
      protocol("protocol", true),
      candidate_type("candidateType", true),
      priority("priority", true),
      url("url", true),
      deleted("deleted", false, true) {}

RTCIceCandidateStats::RTCIceCandidateStats(const RTCIceCandidateStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      transport_id(other.transport_id),
      is_remote(other.is_remote),
      network_type(other.network_type),
      ip(other.ip),
      port(other.port),
      protocol(other.protocol),
      candidate_type(other.candidate_type),
      priority(other.priority),
      url(other.url),
      deleted(other.deleted) {}

RTCIceCandidateStats::~RTCIceCandidateStats() {
}

const char RTCLocalIceCandidateStats::kType[] = "local-candidate";

RTCLocalIceCandidateStats::RTCLocalIceCandidateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCIceCandidateStats(id, timestamp_us, false) {
}

RTCLocalIceCandidateStats::RTCLocalIceCandidateStats(
    std::string&& id, int64_t timestamp_us)
    : RTCIceCandidateStats(std::move(id), timestamp_us, false) {
}

std::unique_ptr<RTCStats> RTCLocalIceCandidateStats::copy() const {
  return std::unique_ptr<RTCStats>(new RTCLocalIceCandidateStats(*this));
}

const char* RTCLocalIceCandidateStats::type() const {
  return kType;
}

const char RTCRemoteIceCandidateStats::kType[] = "remote-candidate";

RTCRemoteIceCandidateStats::RTCRemoteIceCandidateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCIceCandidateStats(id, timestamp_us, true) {
}

RTCRemoteIceCandidateStats::RTCRemoteIceCandidateStats(
    std::string&& id, int64_t timestamp_us)
    : RTCIceCandidateStats(std::move(id), timestamp_us, true) {
}

std::unique_ptr<RTCStats> RTCRemoteIceCandidateStats::copy() const {
  return std::unique_ptr<RTCStats>(new RTCRemoteIceCandidateStats(*this));
}

const char* RTCRemoteIceCandidateStats::type() const {
  return kType;
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCMediaStreamStats, RTCStats, "stream",
    &stream_identifier,
    &track_ids);
// clang-format on

RTCMediaStreamStats::RTCMediaStreamStats(
    const std::string& id, int64_t timestamp_us)
    : RTCMediaStreamStats(std::string(id), timestamp_us) {
}

RTCMediaStreamStats::RTCMediaStreamStats(std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      stream_identifier("streamIdentifier", true),
      track_ids("trackIds", true) {}

RTCMediaStreamStats::RTCMediaStreamStats(
    const RTCMediaStreamStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      stream_identifier(other.stream_identifier),
      track_ids(other.track_ids) {
}

RTCMediaStreamStats::~RTCMediaStreamStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCMediaStreamTrackStats, RTCStats, "track",
                     &track_identifier,
                     &remote_source,
                     &ended,
                     &detached,
                     &kind,
                     &jitter_buffer_delay,
                     &frame_width,
                     &frame_height,
                     &frames_per_second,
                     &frames_sent,
                     &huge_frames_sent,
                     &frames_received,
                     &frames_decoded,
                     &frames_dropped,
                     &frames_corrupted,
                     &partial_frames_lost,
                     &full_frames_lost,
                     &audio_level,
                     &total_audio_energy,
                     &echo_return_loss,
                     &echo_return_loss_enhancement,
                     &total_samples_received,
                     &total_samples_duration,
                     &concealed_samples,
                     &concealment_events);
// clang-format on

RTCMediaStreamTrackStats::RTCMediaStreamTrackStats(
    const std::string& id, int64_t timestamp_us, const char* kind)
    : RTCMediaStreamTrackStats(std::string(id), timestamp_us, kind) {
}

RTCMediaStreamTrackStats::RTCMediaStreamTrackStats(std::string&& id,
                                                   int64_t timestamp_us,
                                                   const char* kind)
    : RTCStats(std::move(id), timestamp_us),
      track_identifier("trackIdentifier", true),
      remote_source("remoteSource", true),
      ended("ended", true),
      detached("detached", true),
      kind("kind", kind, true),
      jitter_buffer_delay("jitterBufferDelay", true),
      frame_width("frameWidth", true),
      frame_height("frameHeight", true),
      frames_per_second("framesPerSecond", true),
      frames_sent("framesSent", true),
      huge_frames_sent("hugeFramesSent", true),
      frames_received("framesReceived", true),
      frames_decoded("framesDecoded", true),
      frames_dropped("framesDropped", true),
      frames_corrupted("framesCorrupted", true),
      partial_frames_lost("partialFramesLost", true),
      full_frames_lost("fullFramesLost", true),
      audio_level("audioLevel", true),
      total_audio_energy("totalAudioEnergy", true),
      echo_return_loss("echoReturnLoss", true),
      echo_return_loss_enhancement("echoReturnLossEnhancement", true),
      total_samples_received("totalSamplesReceived", true),
      total_samples_duration("totalSamplesDuration", true),
      concealed_samples("concealedSamples", true),
      concealment_events("concealmentEvents", true) {
  RTC_DCHECK(kind == RTCMediaStreamTrackKind::kAudio ||
             kind == RTCMediaStreamTrackKind::kVideo);
}

RTCMediaStreamTrackStats::RTCMediaStreamTrackStats(
    const RTCMediaStreamTrackStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      track_identifier(other.track_identifier),
      remote_source(other.remote_source),
      ended(other.ended),
      detached(other.detached),
      kind(other.kind),
      jitter_buffer_delay(other.jitter_buffer_delay),
      frame_width(other.frame_width),
      frame_height(other.frame_height),
      frames_per_second(other.frames_per_second),
      frames_sent(other.frames_sent),
      huge_frames_sent(other.huge_frames_sent),
      frames_received(other.frames_received),
      frames_decoded(other.frames_decoded),
      frames_dropped(other.frames_dropped),
      frames_corrupted(other.frames_corrupted),
      partial_frames_lost(other.partial_frames_lost),
      full_frames_lost(other.full_frames_lost),
      audio_level(other.audio_level),
      total_audio_energy(other.total_audio_energy),
      echo_return_loss(other.echo_return_loss),
      echo_return_loss_enhancement(other.echo_return_loss_enhancement),
      total_samples_received(other.total_samples_received),
      total_samples_duration(other.total_samples_duration),
      concealed_samples(other.concealed_samples),
      concealment_events(other.concealment_events) {}

RTCMediaStreamTrackStats::~RTCMediaStreamTrackStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCPeerConnectionStats, RTCStats, "peer-connection",
    &data_channels_opened,
    &data_channels_closed);
// clang-format on

RTCPeerConnectionStats::RTCPeerConnectionStats(
    const std::string& id, int64_t timestamp_us)
    : RTCPeerConnectionStats(std::string(id), timestamp_us) {
}

RTCPeerConnectionStats::RTCPeerConnectionStats(std::string&& id,
                                               int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      data_channels_opened("dataChannelsOpened", true),
      data_channels_closed("dataChannelsClosed", true) {}

RTCPeerConnectionStats::RTCPeerConnectionStats(
    const RTCPeerConnectionStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      data_channels_opened(other.data_channels_opened),
      data_channels_closed(other.data_channels_closed) {
}

RTCPeerConnectionStats::~RTCPeerConnectionStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCRTPStreamStats, RTCStats, "rtp",
    &ssrc,
    &associate_stats_id,
    &is_remote,
    &media_type,
    &track_id,
    &transport_id,
    &codec_id,
    &fir_count,
    &pli_count,
    &nack_count,
    &sli_count,
    &qp_sum);
// clang-format on

RTCRTPStreamStats::RTCRTPStreamStats(
    const std::string& id, int64_t timestamp_us)
    : RTCRTPStreamStats(std::string(id), timestamp_us) {
}

RTCRTPStreamStats::RTCRTPStreamStats(std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      ssrc("ssrc", true),
      associate_stats_id("associateStatsId", true),
      is_remote("isRemote", false, true),
      media_type("mediaType", true),
      track_id("trackId", true),
      transport_id("transportId", true),
      codec_id("codecId", true),
      fir_count("firCount", true),
      pli_count("pliCount", true),
      nack_count("nackCount", true),
      sli_count("sliCount", true),
      qp_sum("qpSum", true) {}

RTCRTPStreamStats::RTCRTPStreamStats(
    const RTCRTPStreamStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      ssrc(other.ssrc),
      associate_stats_id(other.associate_stats_id),
      is_remote(other.is_remote),
      media_type(other.media_type),
      track_id(other.track_id),
      transport_id(other.transport_id),
      codec_id(other.codec_id),
      fir_count(other.fir_count),
      pli_count(other.pli_count),
      nack_count(other.nack_count),
      sli_count(other.sli_count),
      qp_sum(other.qp_sum) {
}

RTCRTPStreamStats::~RTCRTPStreamStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCInboundRTPStreamStats, RTCRTPStreamStats, "inbound-rtp",
    &packets_received,
    &bytes_received,
    &packets_lost,
    &jitter,
    &fraction_lost,
    &round_trip_time,
    &packets_discarded,
    &packets_repaired,
    &burst_packets_lost,
    &burst_packets_discarded,
    &burst_loss_count,
    &burst_discard_count,
    &burst_loss_rate,
    &burst_discard_rate,
    &gap_loss_rate,
    &gap_discard_rate,
    &frames_decoded);
// clang-format on

RTCInboundRTPStreamStats::RTCInboundRTPStreamStats(
    const std::string& id, int64_t timestamp_us)
    : RTCInboundRTPStreamStats(std::string(id), timestamp_us) {
}

RTCInboundRTPStreamStats::RTCInboundRTPStreamStats(std::string&& id,
                                                   int64_t timestamp_us)
    : RTCRTPStreamStats(std::move(id), timestamp_us),
      packets_received("packetsReceived", true),
      bytes_received("bytesReceived", true),
      packets_lost("packetsLost", true),
      jitter("jitter", true),
      fraction_lost("fractionLost", true),
      round_trip_time("roundTripTime", true),
      packets_discarded("packetsDiscarded", true),
      packets_repaired("packetsRepaired", true),
      burst_packets_lost("burstPacketsLost", true),
      burst_packets_discarded("burstPacketsDiscarded", true),
      burst_loss_count("burstLossCount", true),
      burst_discard_count("burstDiscardCount", true),
      burst_loss_rate("burstLossRate", true),
      burst_discard_rate("burstDiscardRate", true),
      gap_loss_rate("gapLossRate", true),
      gap_discard_rate("gapDiscardRate", true),
      frames_decoded("framesDecoded", true) {}

RTCInboundRTPStreamStats::RTCInboundRTPStreamStats(
    const RTCInboundRTPStreamStats& other)
    : RTCRTPStreamStats(other),
      packets_received(other.packets_received),
      bytes_received(other.bytes_received),
      packets_lost(other.packets_lost),
      jitter(other.jitter),
      fraction_lost(other.fraction_lost),
      round_trip_time(other.round_trip_time),
      packets_discarded(other.packets_discarded),
      packets_repaired(other.packets_repaired),
      burst_packets_lost(other.burst_packets_lost),
      burst_packets_discarded(other.burst_packets_discarded),
      burst_loss_count(other.burst_loss_count),
      burst_discard_count(other.burst_discard_count),
      burst_loss_rate(other.burst_loss_rate),
      burst_discard_rate(other.burst_discard_rate),
      gap_loss_rate(other.gap_loss_rate),
      gap_discard_rate(other.gap_discard_rate),
      frames_decoded(other.frames_decoded) {
}

RTCInboundRTPStreamStats::~RTCInboundRTPStreamStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCOutboundRTPStreamStats, RTCRTPStreamStats, "outbound-rtp",
    &packets_sent,
    &bytes_sent,
    &target_bitrate,
    &frames_encoded);
// clang-format on

RTCOutboundRTPStreamStats::RTCOutboundRTPStreamStats(
    const std::string& id, int64_t timestamp_us)
    : RTCOutboundRTPStreamStats(std::string(id), timestamp_us) {
}

RTCOutboundRTPStreamStats::RTCOutboundRTPStreamStats(std::string&& id,
                                                     int64_t timestamp_us)
    : RTCRTPStreamStats(std::move(id), timestamp_us),
      packets_sent("packetsSent", true),
      bytes_sent("bytesSent", true),
      target_bitrate("targetBitrate", true),
      frames_encoded("framesEncoded", true) {}

RTCOutboundRTPStreamStats::RTCOutboundRTPStreamStats(
    const RTCOutboundRTPStreamStats& other)
    : RTCRTPStreamStats(other),
      packets_sent(other.packets_sent),
      bytes_sent(other.bytes_sent),
      target_bitrate(other.target_bitrate),
      frames_encoded(other.frames_encoded) {
}

RTCOutboundRTPStreamStats::~RTCOutboundRTPStreamStats() {
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCTransportStats, RTCStats, "transport",
    &bytes_sent,
    &bytes_received,
    &rtcp_transport_stats_id,
    &dtls_state,
    &selected_candidate_pair_id,
    &local_certificate_id,
    &remote_certificate_id);
// clang-format on

RTCTransportStats::RTCTransportStats(
    const std::string& id, int64_t timestamp_us)
    : RTCTransportStats(std::string(id), timestamp_us) {
}

RTCTransportStats::RTCTransportStats(std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      bytes_sent("bytesSent", true),
      bytes_received("bytesReceived", true),
      rtcp_transport_stats_id("rtcpTransportStatsId", true),
      dtls_state("dtlsState", true),
      selected_candidate_pair_id("selectedCandidatePairId", true),
      local_certificate_id("localCertificateId", true),
      remote_certificate_id("remoteCertificateId", true) {}

RTCTransportStats::RTCTransportStats(
    const RTCTransportStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      bytes_sent(other.bytes_sent),
      bytes_received(other.bytes_received),
      rtcp_transport_stats_id(other.rtcp_transport_stats_id),
      dtls_state(other.dtls_state),
      selected_candidate_pair_id(other.selected_candidate_pair_id),
      local_certificate_id(other.local_certificate_id),
      remote_certificate_id(other.remote_certificate_id) {
}

RTCTransportStats::~RTCTransportStats() {
}

}  // namespace webrtc
