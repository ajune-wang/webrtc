/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_LEGACY_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_LEGACY_H_

#include <memory>
#include <string>

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder.h"
#include "rtc_base/buffer.h"

#if defined(ENABLE_RTC_EVENT_LOG)

namespace webrtc {

namespace rtclog {
class Event;  // Auto-generated from protobuf.
}  // namespace rtclog

class RtcEventAudioNetworkAdaptation;
class RtcEventAudioPlayout;
class RtcEventAudioReceiveStreamConfig;
class RtcEventAudioSendStreamConfig;
class RtcEventBweUpdateDelayBased;
class RtcEventBweUpdateLossBased;
class RtcEventLoggingStarted;
class RtcEventLoggingStopped;
class RtcEventProbeClusterCreated;
class RtcEventProbeResultFailure;
class RtcEventProbeResultSuccess;
class RtcEventRtcpPacketIncoming;
class RtcEventRtcpPacketOutgoing;
class RtcEventRtpPacketIncoming;
class RtcEventRtpPacketOutgoing;
class RtcEventVideoReceiveStreamConfig;
class RtcEventVideoSendStreamConfig;
class RtpPacket;

class RtcEventLogEncoderLegacy final : public RtcEventLogEncoder {
 public:
  ~RtcEventLogEncoderLegacy() override = default;

  void Encode(const RtcEvent& event) override;

  std::string GetAndResetOutput() override;

 private:
  // Encoding entry-point for the various RtcEvent subclasses.
  void EncodeAudioNetworkAdaptation(
      const RtcEventAudioNetworkAdaptation& event);
  void EncodeAudioPlayout(const RtcEventAudioPlayout& event);
  void EncodeAudioReceiveStreamConfig(
      const RtcEventAudioReceiveStreamConfig& event);
  void EncodeAudioSendStreamConfig(const RtcEventAudioSendStreamConfig& event);
  void EncodeBweUpdateDelayBased(const RtcEventBweUpdateDelayBased& event);
  void EncodeBweUpdateLossBased(const RtcEventBweUpdateLossBased& event);
  void EncodeLoggingStarted(const RtcEventLoggingStarted& event);
  void EncodeLoggingStopped(const RtcEventLoggingStopped& event);
  void EncodeProbeClusterCreated(const RtcEventProbeClusterCreated& event);
  void EncodeProbeResultFailure(const RtcEventProbeResultFailure& event);
  void EncodeProbeResultSuccess(const RtcEventProbeResultSuccess&);
  void EncodeRtcpPacketIncoming(const RtcEventRtcpPacketIncoming& event);
  void EncodeRtcpPacketOutgoing(const RtcEventRtcpPacketOutgoing& event);
  void EncodeRtpPacketIncoming(const RtcEventRtpPacketIncoming& event);
  void EncodeRtpPacketOutgoing(const RtcEventRtpPacketOutgoing& event);
  void EncodeVideoReceiveStreamConfig(
      const RtcEventVideoReceiveStreamConfig& event);
  void EncodeVideoSendStreamConfig(const RtcEventVideoSendStreamConfig& event);

  // RTCP/RTP are handled similarly for incoming/outgoing.
  void EncodeRtcpPacket(int64_t timestamp_us,
                        const rtc::Buffer& packet,
                        bool is_incoming);
  void EncodeRtpPacket(int64_t timestamp_us,
                       const RtpPacket& header,
                       size_t packet_length,
                       int probe_cluster_id,
                       bool is_incoming);

  void Serialize(rtclog::Event* event);

  std::string output_string_;
};

}  // namespace webrtc

#endif  // ENABLE_RTC_EVENT_LOG

#endif  // LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_LEGACY_H_
