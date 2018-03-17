/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_

#include <map>
#include <set>
#include <string>
#include <utility>  // pair
#include <vector>

#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/ignore_wundef.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

enum class BandwidthUsage;
struct AudioEncoderRuntimeConfig;

namespace rtceventlog {

enum PacketDirection { kIncomingPacket = 0, kOutgoingPacket };

struct LoggedAlrStateEvent {
  int64_t timestamp;
  bool in_alr;
};

struct LoggedAudioPlayoutEvent {
  int64_t timestamp;
  uint32_t ssrc;
};

struct LoggedAudioNetworkAdaptationEvent {
  int64_t timestamp;
  AudioEncoderRuntimeConfig config;
};

struct LoggedBweDelayBasedUpdate {
  int64_t timestamp;
  int32_t bitrate_bps;
  BandwidthUsage detector_state;
};

struct LoggedBweLossBasedUpdate {
  int64_t timestamp;
  int32_t new_bitrate;
  uint8_t fraction_lost;
  int32_t expected_packets;
};

struct LoggedBweProbeClusterCreatedEvent {
  int64_t timestamp;
  uint32_t id;
  uint64_t bitrate_bps;
  uint32_t min_packets;
  uint32_t min_bytes;
};

struct LoggedBweProbeResultEvent {
  int64_t timestamp;
  uint32_t id;
  rtc::Optional<uint64_t> bitrate_bps;
  rtc::Optional<ProbeFailureReason> failure_reason;
};

struct LoggedIceCandidatePairConfig {
  int64_t timestamp;
  IceCandidatePairEventType type;
  uint32_t candidate_pair_id;
  IceCandidateType local_candidate_type;
  IceCandidatePairProtocol local_relay_protocol;
  IceCandidateNetworkType local_network_type;
  IceCandidatePairAddressFamily local_address_family;
  IceCandidateType remote_candidate_type;
  IceCandidatePairAddressFamily remote_address_family;
  IceCandidatePairProtocol candidate_pair_protocol;
};

struct LoggedIceCandidatePairEvent {
  int64_t timestamp;
  IceCandidatePairEventType type;
  uint32_t candidate_pair_id;
};

struct LoggedRtpPacket {
  LoggedRtpPacket(uint64_t timestamp, RTPHeader header, size_t total_length)
      : timestamp(timestamp), header(header), total_length(total_length) {}
  int64_t timestamp;
  // TODO(terelius): This allocates space for 15 CSRCs even if none are used.
  RTPHeader header;
  size_t total_length;
};

struct LoggedRtpPacketIncoming : public LoggedRtpPacket {
  LoggedRtpPacketIncoming(uint64_t timestamp,
                          RTPHeader header,
                          size_t total_length)
      : LoggedRtpPacket(timestamp, header, total_length) {}
};

struct LoggedRtpPacketOutgoing : public LoggedRtpPacket {
  LoggedRtpPacketOutgoing(uint64_t timestamp,
                          RTPHeader header,
                          size_t total_length)
      : LoggedRtpPacket(timestamp, header, total_length) {}
};

struct LoggedRtcpPacketIncoming {
  LoggedRtcpPacketIncoming(uint64_t timestamp,
                           const uint8_t* packet,
                           size_t total_length)
      : timestamp(timestamp),
        packet(reinterpret_cast<const char*>(packet), total_length) {}
  int64_t timestamp;
  std::string packet;
};

struct LoggedRtcpPacketOutgoing {
  LoggedRtcpPacketOutgoing(uint64_t timestamp,
                           const uint8_t* packet,
                           size_t total_length)
      : timestamp(timestamp),
        packet(reinterpret_cast<const char*>(packet), total_length) {}
  int64_t timestamp;
  std::string packet;
};

struct LoggedRtcpPacketReceiverReport {
  int64_t timestamp;
  rtcp::ReceiverReport rr;
};

struct LoggedRtcpPacketSenderReport {
  int64_t timestamp;
  rtcp::SenderReport sr;
};

struct LoggedRtcpPacketRemb {
  int64_t timestamp;
  rtcp::Remb remb;
};

struct LoggedRtcpPacketNack {
  int64_t timestamp;
  rtcp::Nack nack;
};

struct LoggedRtcpPacketTransportFeedback {
  int64_t timestamp;
  rtcp::TransportFeedback transport_feedback;
};

struct LoggedStartEvent {
  explicit LoggedStartEvent(uint64_t timestamp) : timestamp(timestamp) {}
  int64_t timestamp;
};

struct LoggedStopEvent {
  explicit LoggedStopEvent(uint64_t timestamp) : timestamp(timestamp) {}
  int64_t timestamp;
};

struct LoggedAudioRecvConfig {
  LoggedAudioRecvConfig(int64_t timestamp, const rtclog::StreamConfig config)
      : timestamp(timestamp), config(config) {}
  int64_t timestamp;
  rtclog::StreamConfig config;
};

struct LoggedAudioSendConfig {
  LoggedAudioSendConfig(int64_t timestamp, const rtclog::StreamConfig config)
      : timestamp(timestamp), config(config) {}
  int64_t timestamp;
  rtclog::StreamConfig config;
};

struct LoggedVideoRecvConfig {
  LoggedVideoRecvConfig(int64_t timestamp, const rtclog::StreamConfig config)
      : timestamp(timestamp), config(config) {}
  int64_t timestamp;
  rtclog::StreamConfig config;
};

struct LoggedVideoSendConfig {
  LoggedVideoSendConfig(int64_t timestamp,
                        const std::vector<rtclog::StreamConfig> configs)
      : timestamp(timestamp), configs(configs) {}
  int64_t timestamp;
  std::vector<rtclog::StreamConfig> configs;
};

// template <typename T>
// class PacketList {
//   typedef iterator
//   iterator
// }

class ParsedRtcEventLog {
  friend class RtcEventLogTestHelper;

 public:
  enum EventType {
    UNKNOWN_EVENT = 0,
    LOG_START = 1,
    LOG_END = 2,
    RTP_EVENT = 3,
    RTCP_EVENT = 4,
    AUDIO_PLAYOUT_EVENT = 5,
    LOSS_BASED_BWE_UPDATE = 6,
    DELAY_BASED_BWE_UPDATE = 7,
    VIDEO_RECEIVER_CONFIG_EVENT = 8,
    VIDEO_SENDER_CONFIG_EVENT = 9,
    AUDIO_RECEIVER_CONFIG_EVENT = 10,
    AUDIO_SENDER_CONFIG_EVENT = 11,
    AUDIO_NETWORK_ADAPTATION_EVENT = 16,
    BWE_PROBE_CLUSTER_CREATED_EVENT = 17,
    BWE_PROBE_RESULT_EVENT = 18,
    ALR_STATE_EVENT = 19,
    ICE_CANDIDATE_PAIR_CONFIG = 20,
    ICE_CANDIDATE_PAIR_EVENT = 21,
  };

  enum class MediaType { ANY, AUDIO, VIDEO, DATA };
  enum class UnconfiguredHeaderExtensions {
    kDontParse,
    kAttemptWebrtcDefaultConfig
  };

  explicit ParsedRtcEventLog(
      UnconfiguredHeaderExtensions parse_unconfigured_header_extensions =
          UnconfiguredHeaderExtensions::kDontParse);

  // Clears previously parsed events and resets the ParsedRtcEventLog to an
  // empty state.
  void Clear();

  // Reads an RtcEventLog file and returns true if parsing was successful.
  bool ParseFile(const std::string& file_name);

  // Reads an RtcEventLog from a string and returns true if successful.
  bool ParseString(const std::string& s);

  // Reads an RtcEventLog from an istream and returns true if successful.
  bool ParseStream(std::istream& stream);

  // Returns the number of events in an EventStream.
  size_t GetNumberOfEvents() const;

  // Reads the arrival timestamp (in microseconds) from a rtclog::Event.
  int64_t GetTimestamp(size_t index) const;
  int64_t GetTimestamp(const rtclog::Event& event) const;

  // Reads the event type of the rtclog::Event at |index|.
  EventType GetEventType(size_t index) const;

  // Reads the header, direction, header length and packet length from the RTP
  // event at |index|, and stores the values in the corresponding output
  // parameters. Each output parameter can be set to nullptr if that value
  // isn't needed.
  // NB: The header must have space for at least IP_PACKET_SIZE bytes.
  // Returns: a pointer to a header extensions map acquired from parsing
  // corresponding Audio/Video Sender/Receiver config events.
  // Warning: if the same SSRC is reused by both video and audio streams during
  // call, extensions maps may be incorrect (the last one would be returned).
  const webrtc::RtpHeaderExtensionMap* GetRtpHeader(
      size_t index,
      PacketDirection* incoming,
      uint8_t* header,
      size_t* header_length,
      size_t* total_length,
      int* probe_cluster_id) const;
  const webrtc::RtpHeaderExtensionMap* GetRtpHeader(
      const rtclog::Event& event,
      PacketDirection* incoming,
      uint8_t* header,
      size_t* header_length,
      size_t* total_length,
      int* probe_cluster_id) const;

  // Reads packet, direction and packet length from the RTCP event at |index|,
  // and stores the values in the corresponding output parameters.
  // Each output parameter can be set to nullptr if that value isn't needed.
  // NB: The packet must have space for at least IP_PACKET_SIZE bytes.
  void GetRtcpPacket(size_t index,
                     PacketDirection* incoming,
                     uint8_t* packet,
                     size_t* length) const;
  void GetRtcpPacket(const rtclog::Event& event,
                     PacketDirection* incoming,
                     uint8_t* packet,
                     size_t* length) const;

  // Reads a video receive config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetVideoReceiveConfig(size_t index) const;
  rtclog::StreamConfig GetVideoReceiveConfig(const rtclog::Event& event) const;

  // Reads a video send config event to a StreamConfig struct. If the proto
  // contains multiple SSRCs and RTX SSRCs (this used to be the case for
  // simulcast streams) then we return one StreamConfig per SSRC,RTX_SSRC pair.
  // Only the fields that are stored in the protobuf will be written.
  std::vector<rtclog::StreamConfig> GetVideoSendConfig(size_t index) const;
  std::vector<rtclog::StreamConfig> GetVideoSendConfig(
      const rtclog::Event& event) const;

  // Reads a audio receive config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetAudioReceiveConfig(size_t index) const;
  rtclog::StreamConfig GetAudioReceiveConfig(const rtclog::Event& event) const;

  // Reads a config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetAudioSendConfig(size_t index) const;
  rtclog::StreamConfig GetAudioSendConfig(const rtclog::Event& event) const;

  // Reads the SSRC from the audio playout event at |index|. The SSRC is stored
  // in the output parameter ssrc. The output parameter can be set to nullptr
  // and in that case the function only asserts that the event is well formed.
  LoggedAudioPlayoutEvent GetAudioPlayout(size_t index) const;
  LoggedAudioPlayoutEvent GetAudioPlayout(const rtclog::Event& event) const;

  // Reads bitrate, fraction loss (as defined in RFC 1889) and total number of
  // expected packets from the loss based BWE event at |index| and stores the
  // values in
  // the corresponding output parameters. Each output parameter can be set to
  // nullptr if that
  // value isn't needed.
  LoggedBweLossBasedUpdate GetLossBasedBweUpdate(size_t index) const;
  LoggedBweLossBasedUpdate GetLossBasedBweUpdate(
      const rtclog::Event& event) const;

  // Reads bitrate and detector_state from the delay based BWE event at |index|
  // and stores the values in the corresponding output parameters. Each output
  // parameter can be set to nullptr if that
  // value isn't needed.
  LoggedBweDelayBasedUpdate GetDelayBasedBweUpdate(size_t index) const;
  LoggedBweDelayBasedUpdate GetDelayBasedBweUpdate(
      const rtclog::Event& event) const;

  // Reads a audio network adaptation event to a (non-NULL)
  // AudioEncoderRuntimeConfig struct. Only the fields that are
  // stored in the protobuf will be written.
  LoggedAudioNetworkAdaptationEvent GetAudioNetworkAdaptation(
      size_t index) const;
  LoggedAudioNetworkAdaptationEvent GetAudioNetworkAdaptation(
      const rtclog::Event& event) const;

  LoggedBweProbeClusterCreatedEvent GetBweProbeClusterCreated(
      size_t index) const;
  LoggedBweProbeClusterCreatedEvent GetBweProbeClusterCreated(
      const rtclog::Event& event) const;

  LoggedBweProbeResultEvent GetBweProbeResult(size_t index) const;
  LoggedBweProbeResultEvent GetBweProbeResult(const rtclog::Event& event) const;

  MediaType GetMediaType(uint32_t ssrc, PacketDirection direction) const;

  LoggedAlrStateEvent GetAlrState(size_t index) const;
  LoggedAlrStateEvent GetAlrState(const rtclog::Event& event) const;

  LoggedIceCandidatePairConfig GetIceCandidatePairConfig(size_t index) const;
  LoggedIceCandidatePairConfig GetIceCandidatePairConfig(
      const rtclog::Event& event) const;

  LoggedIceCandidatePairEvent GetIceCandidatePairEvent(size_t index) const;
  LoggedIceCandidatePairEvent GetIceCandidatePairEvent(
      const rtclog::Event& event) const;

  const std::set<uint32_t>& incoming_rtx_ssrcs() const {
    return incoming_rtx_ssrcs_;
  }
  const std::set<uint32_t>& incoming_video_ssrcs() const {
    return incoming_video_ssrcs_;
  }
  const std::set<uint32_t>& incoming_audio_ssrcs() const {
    return incoming_audio_ssrcs_;
  }
  const std::set<uint32_t>& outgoing_rtx_ssrcs() const {
    return outgoing_rtx_ssrcs_;
  }
  const std::set<uint32_t>& outgoing_video_ssrcs() const {
    return outgoing_video_ssrcs_;
  }
  const std::set<uint32_t>& outgoing_audio_ssrcs() const {
    return outgoing_audio_ssrcs_;
  }

  const std::vector<LoggedStartEvent>& start_log_events() const {
    return start_log_events_;
  }
  const std::vector<LoggedStopEvent>& stop_log_events() const {
    return stop_log_events_;
  }
  const std::map<uint32_t, std::vector<int64_t>>& audio_playout_events() const {
    return audio_playout_events_;
  }
  const std::vector<LoggedAudioNetworkAdaptationEvent>&
  audio_network_adaptation_events() const {
    return audio_network_adaptation_events_;
  }
  const std::vector<LoggedBweProbeClusterCreatedEvent>&
  bwe_probe_cluster_created_events() const {
    return bwe_probe_cluster_created_events_;
  }
  const std::vector<LoggedBweProbeResultEvent>& bwe_probe_result_events()
      const {
    return bwe_probe_result_events_;
  }
  const std::vector<LoggedBweDelayBasedUpdate>& bwe_delay_updates() const {
    return bwe_delay_updates_;
  }
  const std::vector<LoggedBweLossBasedUpdate>& bwe_loss_updates() const {
    return bwe_loss_updates_;
  }
  const std::vector<LoggedAlrStateEvent>& alr_state_events() const {
    return alr_state_events_;
  }
  const std::vector<LoggedIceCandidatePairConfig>& ice_candidate_pair_configs()
      const {
    return ice_candidate_pair_configs_;
  }
  const std::vector<LoggedIceCandidatePairEvent>& ice_candidate_pair_events()
      const {
    return ice_candidate_pair_events_;
  }

  template <typename Direction>
  const std::map<uint32_t, std::vector<typename Direction::RtpPacketType>>&
  rtp_packets() const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }
  template <typename Direction>
  const std::vector<typename Direction::RtcpPacketType>& rtcp_packets() const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }
  template <typename Direction>
  const std::vector<LoggedRtcpPacketReceiverReport>& receiver_reports() const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }
  template <typename Direction>
  const std::vector<LoggedRtcpPacketSenderReport>& sender_reports() const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }
  template <typename Direction>
  const std::vector<LoggedRtcpPacketNack>& nacks() const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }
  template <typename Direction>
  const std::vector<LoggedRtcpPacketRemb>& rembs() const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }
  template <typename Direction>
  const std::vector<LoggedRtcpPacketTransportFeedback>& transport_feedbacks()
      const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }

  int64_t first_timestamp() const { return first_timestamp_; }
  int64_t last_timestamp() const { return last_timestamp_; }

 private:
  void StoreParsedEvent(const rtclog::Event& event);

  std::vector<rtclog::Event> events_;

  struct Stream {
    Stream(uint32_t ssrc,
           MediaType media_type,
           PacketDirection direction,
           webrtc::RtpHeaderExtensionMap map)
        : ssrc(ssrc),
          media_type(media_type),
          direction(direction),
          rtp_extensions_map(map) {}
    uint32_t ssrc;
    MediaType media_type;
    PacketDirection direction;
    webrtc::RtpHeaderExtensionMap rtp_extensions_map;
  };

  const UnconfiguredHeaderExtensions parse_unconfigured_header_extensions_;

  // Make a default extension map for streams without configuration information.
  // TODO(ivoc): Once configuration of audio streams is stored in the event log,
  //             this can be removed. Tracking bug: webrtc:6399
  RtpHeaderExtensionMap default_extension_map_;

  // Tracks what each stream is configured for. Note that a single SSRC can be
  // in several sets. For example, the SSRC used for sending video over RTX
  // will appear in both video_ssrcs_ and rtx_ssrcs_. In the unlikely case that
  // an SSRC is reconfigured to a different media type mid-call, it will also
  // appear in multiple sets.
  std::set<uint32_t> incoming_rtx_ssrcs_;
  std::set<uint32_t> incoming_video_ssrcs_;
  std::set<uint32_t> incoming_audio_ssrcs_;
  std::set<uint32_t> outgoing_rtx_ssrcs_;
  std::set<uint32_t> outgoing_video_ssrcs_;
  std::set<uint32_t> outgoing_audio_ssrcs_;

  // Maps an SSRC to the parsed  RTP headers in that stream. Header extensions
  // are parsed if the stream has been configured.
  std::map<uint32_t, std::vector<LoggedRtpPacketIncoming>>
      incoming_rtp_packets_;
  std::map<uint32_t, std::vector<LoggedRtpPacketOutgoing>>
      outgoing_rtp_packets_;
  // Raw RTCP packets.
  std::vector<LoggedRtcpPacketIncoming> incoming_rtcp_packets_;
  std::vector<LoggedRtcpPacketOutgoing> outgoing_rtcp_packets_;
  // Parsed RTCP messages. Currently not separated based on SSRC.
  std::vector<LoggedRtcpPacketReceiverReport> incoming_rr_;
  std::vector<LoggedRtcpPacketReceiverReport> outgoing_rr_;
  std::vector<LoggedRtcpPacketSenderReport> incoming_sr_;
  std::vector<LoggedRtcpPacketSenderReport> outgoing_sr_;
  std::vector<LoggedRtcpPacketNack> incoming_nack_;
  std::vector<LoggedRtcpPacketNack> outgoing_nack_;
  std::vector<LoggedRtcpPacketRemb> incoming_remb_;
  std::vector<LoggedRtcpPacketRemb> outgoing_remb_;
  std::vector<LoggedRtcpPacketTransportFeedback> incoming_transport_feedback_;
  std::vector<LoggedRtcpPacketTransportFeedback> outgoing_transport_feedback_;

  uint8_t last_incoming_rtcp_packet_[IP_PACKET_SIZE];
  uint8_t last_incoming_rtcp_packet_length_;

  std::vector<LoggedStartEvent> start_log_events_;
  std::vector<LoggedStopEvent> stop_log_events_;

  // Maps an SSRC to the timestamps of parsed audio playout events.
  std::map<uint32_t, std::vector<int64_t>> audio_playout_events_;

  std::vector<LoggedAudioNetworkAdaptationEvent>
      audio_network_adaptation_events_;

  std::vector<LoggedBweProbeClusterCreatedEvent>
      bwe_probe_cluster_created_events_;

  std::vector<LoggedBweProbeResultEvent> bwe_probe_result_events_;

  std::vector<LoggedBweDelayBasedUpdate> bwe_delay_updates_;

  // A list of all updates from the send-side loss-based bandwidth estimator.
  std::vector<LoggedBweLossBasedUpdate> bwe_loss_updates_;

  std::vector<LoggedAlrStateEvent> alr_state_events_;

  std::vector<LoggedIceCandidatePairConfig> ice_candidate_pair_configs_;

  std::vector<LoggedIceCandidatePairEvent> ice_candidate_pair_events_;

  std::vector<LoggedAudioRecvConfig> audio_recv_configs_;
  std::vector<LoggedAudioSendConfig> audio_send_configs_;
  std::vector<LoggedVideoRecvConfig> video_recv_configs_;
  std::vector<LoggedVideoSendConfig> video_send_configs_;

  int64_t first_timestamp_;
  int64_t last_timestamp_;

  // The extension maps are mutable to allow us to insert the default
  // configuration when parsing an RTP header for an unconfigured stream.
  mutable std::map<uint32_t, webrtc::RtpHeaderExtensionMap>
      incoming_rtp_extensions_maps_;
  mutable std::map<uint32_t, webrtc::RtpHeaderExtensionMap>
      outgoing_rtp_extensions_maps_;
};

struct Incoming;
struct Outgoing;

struct Incoming {
  using RtpPacketType = LoggedRtpPacketIncoming;
  using RtcpPacketType = LoggedRtcpPacketIncoming;
  using ReverseDirection = Outgoing;
  // static const PacketDirection direction = kIncomingPacket;
  static constexpr char name[] = "In";
  static constexpr char full_name[] = "Incoming";
};

struct Outgoing {
  using RtpPacketType = LoggedRtpPacketOutgoing;
  using RtcpPacketType = LoggedRtcpPacketOutgoing;
  using ReverseDirection = Incoming;
  // static const PacketDirection direction = kOutgoingPacket;
  static constexpr char name[] = "Out";
  static constexpr char full_name[] = "Outgoing";
};

template <>
inline const std::map<uint32_t, std::vector<Incoming::RtpPacketType>>&
ParsedRtcEventLog::rtp_packets<Incoming>() const {
  return incoming_rtp_packets_;
}

template <>
inline const std::map<uint32_t, std::vector<Outgoing::RtpPacketType>>&
ParsedRtcEventLog::rtp_packets<Outgoing>() const {
  return outgoing_rtp_packets_;
}

template <>
inline const std::vector<Incoming::RtcpPacketType>&
ParsedRtcEventLog::rtcp_packets<Incoming>() const {
  return incoming_rtcp_packets_;
}

template <>
inline const std::vector<Outgoing::RtcpPacketType>&
ParsedRtcEventLog::rtcp_packets<Outgoing>() const {
  return outgoing_rtcp_packets_;
}

template <>
inline const std::vector<LoggedRtcpPacketReceiverReport>&
ParsedRtcEventLog::receiver_reports<Incoming>() const {
  return incoming_rr_;
}

template <>
inline const std::vector<LoggedRtcpPacketReceiverReport>&
ParsedRtcEventLog::receiver_reports<Outgoing>() const {
  return outgoing_rr_;
}

template <>
inline const std::vector<LoggedRtcpPacketSenderReport>&
ParsedRtcEventLog::sender_reports<Incoming>() const {
  return incoming_sr_;
}

template <>
inline const std::vector<LoggedRtcpPacketSenderReport>&
ParsedRtcEventLog::sender_reports<Outgoing>() const {
  return outgoing_sr_;
}

template <>
inline const std::vector<LoggedRtcpPacketNack>&
ParsedRtcEventLog::nacks<Incoming>() const {
  return incoming_nack_;
}

template <>
inline const std::vector<LoggedRtcpPacketNack>&
ParsedRtcEventLog::nacks<Outgoing>() const {
  return outgoing_nack_;
}

template <>
inline const std::vector<LoggedRtcpPacketRemb>&
ParsedRtcEventLog::rembs<Incoming>() const {
  return incoming_remb_;
}

template <>
inline const std::vector<LoggedRtcpPacketRemb>&
ParsedRtcEventLog::rembs<Outgoing>() const {
  return outgoing_remb_;
}

template <>
inline const std::vector<LoggedRtcpPacketTransportFeedback>&
ParsedRtcEventLog::transport_feedbacks<Incoming>() const {
  return incoming_transport_feedback_;
}

template <>
inline const std::vector<LoggedRtcpPacketTransportFeedback>&
ParsedRtcEventLog::transport_feedbacks<Outgoing>() const {
  return outgoing_transport_feedback_;
}

}  // namespace rtceventlog
}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_
