/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_to_text/converter.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/transport/network_types.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "logging/rtc_event_log/events/rtc_event_frame_decoded.h"
#include "logging/rtc_event_log/events/rtc_event_generic_ack_received.h"
#include "logging/rtc_event_log/events/rtc_event_generic_packet_received.h"
#include "logging/rtc_event_log/events/rtc_event_generic_packet_sent.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_remote_estimate.h"
#include "logging/rtc_event_log/events/rtc_event_route_change.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_processor.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {
namespace {

void PrintHeaderExtensionConfig(
    FILE* output,
    const std::vector<RtpExtension>& rtp_extensions) {
  if (rtp_extensions.empty())
    return;
  fprintf(output, " extension_map=");
  for (const RtpExtension& extension : rtp_extensions) {
    fprintf(output, "{uri:%s,id:%d}", extension.uri.c_str(), extension.id);
  }
}

}  // namespace

bool Convert(
    std::string inputfile,
    FILE* output,
    ParsedRtcEventLog::UnconfiguredHeaderExtensions unconfigured_extensions) {
  ParsedRtcEventLog parsed_log(unconfigured_extensions,
                               /*allow_incomplete_logs*/ true);

  auto status = parsed_log.ParseFile(inputfile);
  if (!status.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to parse " << inputfile << ": "
                      << status.message();
    return false;
  }

  auto audio_recv_stream_handler = [&](const LoggedAudioRecvConfig& event) {
    fprintf(output, "AUDIO_RECV_STREAM_CONFIG %ld", event.log_time_ms());
    fprintf(output, " remote_ssrc=%u", event.config.remote_ssrc);
    fprintf(output, " local_ssrc=%u", event.config.local_ssrc);
    PrintHeaderExtensionConfig(output, event.config.rtp_extensions);
    fprintf(output, "\n");
  };

  auto audio_send_stream_handler = [&](const LoggedAudioSendConfig& event) {
    fprintf(output, "AUDIO_SEND_STREAM_CONFIG %ld", event.log_time_ms());
    fprintf(output, " ssrc=%u", event.config.local_ssrc);
    PrintHeaderExtensionConfig(output, event.config.rtp_extensions);
    fprintf(output, "\n");
  };

  auto video_recv_stream_handler = [&](const LoggedVideoRecvConfig& event) {
    fprintf(output, "VIDEO_RECV_STREAM_CONFIG %ld", event.log_time_ms());
    fprintf(output, " remote_ssrc=%u", event.config.remote_ssrc);
    fprintf(output, " local_ssrc=%u", event.config.local_ssrc);
    fprintf(output, " rtx_ssrc=%u", event.config.rtx_ssrc);
    PrintHeaderExtensionConfig(output, event.config.rtp_extensions);
    fprintf(output, "\n");
  };

  auto video_send_stream_handler = [&](const LoggedVideoSendConfig& event) {
    fprintf(output, "VIDEO_SEND_STREAM_CONFIG %ld", event.log_time_ms());
    fprintf(output, " ssrc=%u", event.config.local_ssrc);
    fprintf(output, " rtx_ssrc=%u", event.config.rtx_ssrc);
    PrintHeaderExtensionConfig(output, event.config.rtp_extensions);
    fprintf(output, "\n");
  };

  auto start_logging_handler = [&](const LoggedStartEvent& event) {
    fprintf(output, "START_LOG %ld\n", event.log_time_ms());
  };

  auto stop_logging_handler = [&](const LoggedStopEvent& event) {
    fprintf(output, "STOP_LOG %ld\n", event.log_time_ms());
  };

  auto alr_state_handler = [&](const LoggedAlrStateEvent& event) {
    fprintf(output, "STOP_LOG %ld\n", event.log_time_ms());
  };

  auto audio_playout_handler = [&](const LoggedAudioPlayoutEvent& event) {
    fprintf(output, "AUDIO_PLAYOUT %ld ssrc=%u\n", event.log_time_ms(),
            event.ssrc);
  };

  auto audio_network_adaptation_handler =
      [&](const LoggedAudioNetworkAdaptationEvent& event) {
        // TODO(terelius): Implement
        fprintf(output, "AUDIO_NETWORK_ADAPTATION %ld ...\n",
                event.log_time_ms());
      };

  auto bwe_probe_cluster_created_handler =
      [&](const LoggedBweProbeClusterCreatedEvent& event) {
        fprintf(output,
                "BWE_PROBE_CREATED %ld id=%u bitrate_bps=%d min_packets=%u "
                "min_bytes=%u\n",
                event.log_time_ms(), event.id, event.bitrate_bps,
                event.min_packets, event.min_bytes);
      };

  auto bwe_probe_failure_handler =
      [&](const LoggedBweProbeFailureEvent& event) {
        fprintf(output, "BWE_PROBE_FAILURE %ld id=%d reason=%d\n",
                event.log_time_ms(), event.id, event.failure_reason);
      };

  auto bwe_probe_success_handler =
      [&](const LoggedBweProbeSuccessEvent& event) {
        fprintf(output, "BWE_PROBE_SUCCESS %ld id=%u bitrate_bps=%d\n",
                event.log_time_ms(), event.id, event.bitrate_bps);
      };

  auto bwe_delay_update_handler = [&](const LoggedBweDelayBasedUpdate& event) {
    fprintf(output, "BWE_DELAY_BASED %ld bitrate_bps=%u detector_state=%d\n",
            event.log_time_ms(), event.bitrate_bps, event.detector_state);
  };

  auto bwe_loss_update_handler = [&](const LoggedBweLossBasedUpdate& event) {
    fprintf(output,
            "BWE_LOSS_BASED %ld bitrate_bps=%u fraction_lost=%d "
            "expected_packets=%d\n",
            event.log_time_ms(), event.bitrate_bps, event.fraction_lost,
            event.expected_packets);
  };

  auto dtls_transport_state_handler =
      [&](const LoggedDtlsTransportState& event) {
        fprintf(output, "DTLS_TRANSPORT_STATE %ld state=%d\n",
                event.log_time_ms(), event.dtls_transport_state);
      };

  auto dtls_transport_writable_handler =
      [&](const LoggedDtlsWritableState& event) {
        fprintf(output, "DTLS_WRITABLE %ld writable=%d\n", event.log_time_ms(),
                event.writable);
      };

  auto ice_candidate_pair_config_handler =
      [&](const LoggedIceCandidatePairConfig& event) {
        // TODO(terelius): Implement
        fprintf(output, "ICE_CANDIDATE_CONFIG %ld\n", event.log_time_ms());
      };

  auto ice_candidate_pair_event_handler =
      [&](const LoggedIceCandidatePairEvent& event) {
        // TODO(terelius): Implement
        fprintf(output, "ICE_CANDIDATE_UPDATE %ld\n", event.log_time_ms());
      };

  auto route_change_handler = [&](const LoggedRouteChangeEvent& event) {
    fprintf(output, "ROUTE_CHANGE %ld connected=%d overhead=%d\n",
            event.log_time_ms(), event.connected, event.overhead);
  };

  auto remote_estimate_handler = [&](const LoggedRemoteEstimateEvent& event) {
    fprintf(output, "REMOTE_ESTIMATE %ld", event.log_time_ms());
    if (event.link_capacity_lower.has_value())
      fprintf(output, " link_capacity_lower_kbps=%ld",
              event.link_capacity_lower.value().kbps());
    if (event.link_capacity_upper.has_value())
      fprintf(output, " link_capacity_upper_kbps=%ld",
              event.link_capacity_upper.value().kbps());
    fprintf(output, "\n");
  };

  auto incoming_rtp_packet_handler = [&](const LoggedRtpPacketIncoming& event) {
    fprintf(output, "RTP_IN %ld", event.log_time_ms());
    fprintf(output, " ssrc=%u", event.rtp.header.ssrc);
    fprintf(output, " seq_no=%u", event.rtp.header.sequenceNumber);
    fprintf(output, " marker=%u", event.rtp.header.markerBit);
    fprintf(output, " pt=%u", event.rtp.header.payloadType);
    fprintf(output, " timestamp=%u", event.rtp.header.timestamp);
    if (event.rtp.header.extension.hasAbsoluteSendTime) {
      fprintf(output, " abs_send_time=%u",
              event.rtp.header.extension.absoluteSendTime);
    }
    if (event.rtp.header.extension.hasTransmissionTimeOffset) {
      fprintf(output, " transmission_offset=%d",
              event.rtp.header.extension.transmissionTimeOffset);
    }
    if (event.rtp.header.extension.hasAudioLevel) {
      fprintf(output, " voice_activity=%d",
              event.rtp.header.extension.voiceActivity);
      fprintf(output, " audio_level=%u", event.rtp.header.extension.audioLevel);
    }
    if (event.rtp.header.extension.hasVideoRotation) {
      fprintf(output, " video_rotation=%d",
              event.rtp.header.extension.videoRotation);
    }
    if (event.rtp.header.extension.hasTransportSequenceNumber) {
      fprintf(output, " transport_seq_no=%u",
              event.rtp.header.extension.transportSequenceNumber);
    }
    fprintf(output, " header_length=%zu", event.rtp.header_length);
    fprintf(output, " padding_length=%zu", event.rtp.header.paddingLength);
    fprintf(output, " total_length=%zu", event.rtp.total_length);
    fprintf(output, "\n");
  };

  auto outgoing_rtp_packet_handler = [&](const LoggedRtpPacketOutgoing& event) {
    fprintf(output, "RTP_OUT %ld", event.log_time_ms());
    fprintf(output, " ssrc=%u", event.rtp.header.ssrc);
    fprintf(output, " seq_no=%u", event.rtp.header.sequenceNumber);
    fprintf(output, " marker=%u", event.rtp.header.markerBit);
    fprintf(output, " pt=%u", event.rtp.header.payloadType);
    fprintf(output, " timestamp=%u", event.rtp.header.timestamp);
    if (event.rtp.header.extension.hasAbsoluteSendTime) {
      fprintf(output, " abs_send_time=%u",
              event.rtp.header.extension.absoluteSendTime);
    }
    if (event.rtp.header.extension.hasTransmissionTimeOffset) {
      fprintf(output, " transmission_offset=%d",
              event.rtp.header.extension.transmissionTimeOffset);
    }
    if (event.rtp.header.extension.hasAudioLevel) {
      fprintf(output, " voice_activity=%d",
              event.rtp.header.extension.voiceActivity);
      fprintf(output, " audio_level=%u", event.rtp.header.extension.audioLevel);
    }
    if (event.rtp.header.extension.hasVideoRotation) {
      fprintf(output, " video_rotation=%d",
              event.rtp.header.extension.videoRotation);
    }
    if (event.rtp.header.extension.hasTransportSequenceNumber) {
      fprintf(output, " transport_seq_no=%u",
              event.rtp.header.extension.transportSequenceNumber);
    }
    fprintf(output, " header_length=%zu", event.rtp.header_length);
    fprintf(output, " padding_length=%zu", event.rtp.header.paddingLength);
    fprintf(output, " total_length=%zu", event.rtp.total_length);
    fprintf(output, "\n");
  };

  auto incoming_rtcp_packet_handler =
      [&](const LoggedRtcpPacketIncoming& event) {
        fprintf(output, "RTCP_IN %ld\n", event.log_time_ms());
      };

  auto outgoing_rtcp_packet_handler =
      [&](const LoggedRtcpPacketOutgoing& event) {
        fprintf(output, "RTCP_OUT %ld\n", event.log_time_ms());
      };

  auto generic_packet_received_handler =
      [&](const LoggedGenericPacketReceived& event) {
        fprintf(output, "GENERIC_PACKET_RECV %ld packet_no=%ld length=%d\n",
                event.log_time_ms(), event.packet_number, event.packet_length);
      };

  auto generic_packet_sent_handler = [&](const LoggedGenericPacketSent& event) {
    fprintf(output,
            "GENERIC_PACKET_SENT %ld packet_no=%ld overhead_length=%zu "
            "payload_length=%zu padding_length=%zu\n",
            event.log_time_ms(), event.packet_number, event.overhead_length,
            event.payload_length, event.padding_length);
  };

  auto generic_ack_received_handler =
      [&](const LoggedGenericAckReceived& event) {
        // TODO(terelius): Implement
        fprintf(output, "GENERIC_ACK_RECV %ld\n", event.log_time_ms());
      };

  auto decoded_frame_handler = [&](const LoggedFrameDecoded& event) {
    fprintf(output,
            "FRAME_DECODED %ld render_time=%ld ssrc=%u width=%d height=%d "
            "codec=%d qp=%u\n",
            event.log_time_ms(), event.render_time_ms, event.ssrc, event.width,
            event.height, event.codec, event.qp);
  };

  RtcEventProcessor processor;

  processor.AddEvents(parsed_log.audio_recv_configs(),
                      audio_recv_stream_handler);
  processor.AddEvents(parsed_log.audio_send_configs(),
                      audio_send_stream_handler);
  processor.AddEvents(parsed_log.video_recv_configs(),
                      video_recv_stream_handler);
  processor.AddEvents(parsed_log.video_send_configs(),
                      video_send_stream_handler);

  processor.AddEvents(parsed_log.start_log_events(), start_logging_handler);
  processor.AddEvents(parsed_log.stop_log_events(), stop_logging_handler);

  processor.AddEvents(parsed_log.alr_state_events(), alr_state_handler);

  for (const auto& kv : parsed_log.audio_playout_events()) {
    processor.AddEvents(kv.second, audio_playout_handler);
  }

  processor.AddEvents(parsed_log.audio_network_adaptation_events(),
                      audio_network_adaptation_handler);
  processor.AddEvents(parsed_log.bwe_probe_cluster_created_events(),
                      bwe_probe_cluster_created_handler);
  processor.AddEvents(parsed_log.bwe_probe_failure_events(),
                      bwe_probe_failure_handler);
  processor.AddEvents(parsed_log.bwe_probe_success_events(),
                      bwe_probe_success_handler);

  processor.AddEvents(parsed_log.bwe_delay_updates(), bwe_delay_update_handler);
  processor.AddEvents(parsed_log.bwe_loss_updates(), bwe_loss_update_handler);

  processor.AddEvents(parsed_log.dtls_transport_states(),
                      dtls_transport_state_handler);
  processor.AddEvents(parsed_log.dtls_writable_states(),
                      dtls_transport_writable_handler);
  processor.AddEvents(parsed_log.ice_candidate_pair_configs(),
                      ice_candidate_pair_config_handler);
  processor.AddEvents(parsed_log.ice_candidate_pair_events(),
                      ice_candidate_pair_event_handler);
  processor.AddEvents(parsed_log.route_change_events(), route_change_handler);
  processor.AddEvents(parsed_log.remote_estimate_events(),
                      remote_estimate_handler);

  for (const auto& stream : parsed_log.incoming_rtp_packets_by_ssrc()) {
    processor.AddEvents(stream.incoming_packets, incoming_rtp_packet_handler);
  }
  for (const auto& stream : parsed_log.outgoing_rtp_packets_by_ssrc()) {
    processor.AddEvents(stream.outgoing_packets, outgoing_rtp_packet_handler);
  }

  processor.AddEvents(parsed_log.incoming_rtcp_packets(),
                      incoming_rtcp_packet_handler);

  processor.AddEvents(parsed_log.outgoing_rtcp_packets(),
                      outgoing_rtcp_packet_handler);

  processor.AddEvents(parsed_log.generic_packets_received(),
                      generic_packet_received_handler);
  processor.AddEvents(parsed_log.generic_packets_sent(),
                      generic_packet_sent_handler);
  processor.AddEvents(parsed_log.generic_acks_received(),
                      generic_ack_received_handler);

  for (const auto& kv : parsed_log.decoded_frames()) {
    processor.AddEvents(kv.second, decoded_frame_handler);
  }

  processor.ProcessEventsInOrder();

  return true;
}

}  // namespace webrtc
