/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/ecn_marking.h"

namespace webrtc {

constexpr TimeDelta kSendTimeHistoryWindow = TimeDelta::Seconds(60);

PacketFeedback CreatePacketFeedback(const rtc::NetworkRoute& network_route,
                                    const RtpPacketToSend& packet,
                                    const PacedPacketInfo& pacing_info,
                                    size_t overhead_bytes,
                                    Timestamp creation_time) {
  PacketFeedback feedback;

  feedback.creation_time = creation_time;
  feedback.sent.sequence_number = *packet.transport_sequence_number();
  feedback.sent.size = DataSize::Bytes(packet.size() + overhead_bytes);
  feedback.sent.audio = packet.packet_type() == RtpPacketMediaType::kAudio;
  feedback.network_route = network_route;
  feedback.sent.pacing_info = pacing_info;
  feedback.ssrc = packet.Ssrc();
  feedback.rtp_sequence_number = packet.SequenceNumber();
  return feedback;
}

void InFlightBytesTracker::AddInFlightPacketBytes(
    const PacketFeedback& packet) {
  RTC_DCHECK(packet.sent.send_time.IsFinite());
  auto it = in_flight_data_.find(packet.network_route);
  if (it != in_flight_data_.end()) {
    it->second += packet.sent.size;
  } else {
    in_flight_data_.insert({packet.network_route, packet.sent.size});
  }
}

void InFlightBytesTracker::RemoveInFlightPacketBytes(
    const PacketFeedback& packet) {
  if (packet.sent.send_time.IsInfinite())
    return;
  auto it = in_flight_data_.find(packet.network_route);
  if (it != in_flight_data_.end()) {
    RTC_DCHECK_GE(it->second, packet.sent.size);
    it->second -= packet.sent.size;
    if (it->second.IsZero())
      in_flight_data_.erase(it);
  }
}

DataSize InFlightBytesTracker::GetOutstandingData(
    const rtc::NetworkRoute& network_route) const {
  auto it = in_flight_data_.find(network_route);
  if (it != in_flight_data_.end()) {
    return it->second;
  } else {
    return DataSize::Zero();
  }
}

// Comparator for consistent map with NetworkRoute as key.
bool InFlightBytesTracker::NetworkRouteComparator::operator()(
    const rtc::NetworkRoute& a,
    const rtc::NetworkRoute& b) const {
  if (a.local.network_id() != b.local.network_id())
    return a.local.network_id() < b.local.network_id();
  if (a.remote.network_id() != b.remote.network_id())
    return a.remote.network_id() < b.remote.network_id();

  if (a.local.adapter_id() != b.local.adapter_id())
    return a.local.adapter_id() < b.local.adapter_id();
  if (a.remote.adapter_id() != b.remote.adapter_id())
    return a.remote.adapter_id() < b.remote.adapter_id();

  if (a.local.uses_turn() != b.local.uses_turn())
    return a.local.uses_turn() < b.local.uses_turn();
  if (a.remote.uses_turn() != b.remote.uses_turn())
    return a.remote.uses_turn() < b.remote.uses_turn();

  return a.connected < b.connected;
}

TransportFeedbackAdapter::TransportFeedbackAdapter() = default;

void TransportFeedbackAdapter::AddPacket(const RtpPacketToSend& packet,
                                         const PacedPacketInfo& pacing_info,
                                         size_t overhead_bytes,
                                         Timestamp creation_time) {
  // Assume transport sequence number header extension is used unless
  // packet.transport_sequence_number() is set and the packet does not contain
  // the extension.
  use_transport_sequence_number_header_extension_ =
      !(packet.transport_sequence_number() &&
        !packet.HasExtension<TransportSequenceNumber>());
  StorePacketFeedback(CreatePacketFeedback(network_route_, packet, pacing_info,
                                           overhead_bytes, creation_time));
}

void TransportFeedbackAdapter::AddPacket(const RtpPacketSendInfo& packet_info,
                                         size_t overhead_bytes,
                                         Timestamp creation_time) {
  PacketFeedback packet;
  packet.creation_time = creation_time;
  packet.sent.sequence_number =
      seq_num_unwrapper_.Unwrap(packet_info.transport_sequence_number);
  packet.sent.size = DataSize::Bytes(packet_info.length + overhead_bytes);
  packet.sent.audio = packet_info.packet_type == RtpPacketMediaType::kAudio;
  packet.network_route = network_route_;
  packet.sent.pacing_info = packet_info.pacing_info;
  StorePacketFeedback(packet);
}

void TransportFeedbackAdapter::StorePacketFeedback(
    const PacketFeedback& packet) {
  while (!history_.empty() &&
         packet.creation_time - history_.begin()->second.creation_time >
             kSendTimeHistoryWindow) {
    // TODO(sprang): Warn if erasing (too many) old items?
    if (history_.begin()->second.sent.sequence_number > last_ack_seq_num_)
      in_flight_.RemoveInFlightPacketBytes(history_.begin()->second);

    const PacketFeedback& packet = history_.begin()->second;
    rtp_to_transport_sequence_number_.erase(
        SsrcRtpSequenceNumberPair(packet.ssrc, packet.rtp_sequence_number));
    history_.erase(history_.begin());
  }
  SsrcRtpSequenceNumberPair ssrc_seq_pair =
      std::make_pair(packet.ssrc, packet.rtp_sequence_number);
  // Note that it can happen that the same SSRC and sequence number is sent
  // again. I.E audio retransmission.
  rtp_to_transport_sequence_number_.insert(
      std::make_pair(ssrc_seq_pair, packet.sent.sequence_number));
  history_.insert(std::make_pair(packet.sent.sequence_number, packet));
}

std::optional<SentPacket> TransportFeedbackAdapter::ProcessSentPacket(
    const rtc::SentPacket& sent_packet) {
  auto send_time = Timestamp::Millis(sent_packet.send_time_ms);
  // TODO(srte): Only use one way to indicate that packet feedback is used.
  if (sent_packet.info.included_in_feedback || sent_packet.packet_id != -1) {
    // Despite rtc::SentPacket.packet_id beeing 64bit, only 16 bit is used,
    // if transport sequence number header extension is used,
    // see RtpSenderEgress::CompleteSendPacket.
    int64_t unwrapped_seq_num =
        use_transport_sequence_number_header_extension_
            ? seq_num_unwrapper_.Unwrap(sent_packet.packet_id)
            : sent_packet.packet_id;

    auto it = history_.find(unwrapped_seq_num);
    if (it != history_.end()) {
      bool packet_retransmit = it->second.sent.send_time.IsFinite();
      it->second.sent.send_time = send_time;
      last_send_time_ = std::max(last_send_time_, send_time);
      // TODO(srte): Don't do this on retransmit.
      if (!pending_untracked_size_.IsZero()) {
        if (send_time < last_untracked_send_time_)
          RTC_LOG(LS_WARNING)
              << "appending acknowledged data for out of order packet. (Diff: "
              << ToString(last_untracked_send_time_ - send_time) << " ms.)";
        it->second.sent.prior_unacked_data += pending_untracked_size_;
        pending_untracked_size_ = DataSize::Zero();
      }
      if (!packet_retransmit) {
        if (it->second.sent.sequence_number > last_ack_seq_num_)
          in_flight_.AddInFlightPacketBytes(it->second);
        it->second.sent.data_in_flight = GetOutstandingData();
        return it->second.sent;
      }
    }
  } else if (sent_packet.info.included_in_allocation) {
    if (send_time < last_send_time_) {
      RTC_LOG(LS_WARNING) << "ignoring untracked data for out of order packet.";
    }
    pending_untracked_size_ +=
        DataSize::Bytes(sent_packet.info.packet_size_bytes);
    last_untracked_send_time_ = std::max(last_untracked_send_time_, send_time);
  }
  return std::nullopt;
}

std::optional<PacketFeedback> TransportFeedbackAdapter::RetrievePacketFeedback(
    int64_t seq_num,
    bool received) {
  if (seq_num > last_ack_seq_num_) {
    // Starts at history_.begin() if last_ack_seq_num_ < 0, since any
    // valid sequence number is >= 0.
    for (auto it = history_.upper_bound(last_ack_seq_num_);
         it != history_.upper_bound(seq_num); ++it) {
      in_flight_.RemoveInFlightPacketBytes(it->second);
    }
    last_ack_seq_num_ = seq_num;
  }

  auto it = history_.find(seq_num);
  if (it == history_.end()) {
    RTC_LOG(LS_WARNING) << "Failed to lookup send time for packet with "
                        << seq_num << ". Send time history too small?";
    return std::nullopt;
  }

  if (it->second.sent.send_time.IsInfinite()) {
    // TODO(srte): Fix the tests that makes this happen and make this a
    // DCHECK.
    RTC_DLOG(LS_ERROR)
        << "Received feedback before packet was indicated as sent";
    return std::nullopt;
  }

  PacketFeedback packet_feedback = it->second;
  if (received) {
    // Note: Lost packets are not removed from history because they might
    // be reported as received by a later feedback.
    rtp_to_transport_sequence_number_.erase(SsrcRtpSequenceNumberPair(
        packet_feedback.ssrc, packet_feedback.rtp_sequence_number));
    history_.erase(it);
  }
  return packet_feedback;
}

std::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessTransportFeedback(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_receive_time) {
  if (feedback.GetPacketStatusCount() == 0) {
    RTC_LOG(LS_INFO) << "Empty transport feedback packet received.";
    return std::nullopt;
  }

  TransportPacketsFeedback msg;
  msg.feedback_time = feedback_receive_time;
  msg.packet_feedbacks =
      ProcessTransportFeedbackInner(feedback, feedback_receive_time);
  if (msg.packet_feedbacks.empty())
    return std::nullopt;
  msg.data_in_flight = in_flight_.GetOutstandingData(network_route_);

  return msg;
}

void TransportFeedbackAdapter::SetNetworkRoute(
    const rtc::NetworkRoute& network_route) {
  network_route_ = network_route;
}

DataSize TransportFeedbackAdapter::GetOutstandingData() const {
  return in_flight_.GetOutstandingData(network_route_);
}

std::vector<PacketResult>
TransportFeedbackAdapter::ProcessTransportFeedbackInner(
    const rtcp::TransportFeedback& feedback,
    Timestamp feedback_receive_time) {
  // Add timestamp deltas to a local time base selected on first packet arrival.
  // This won't be the true time base, but makes it easier to manually inspect
  // time stamps.
  if (last_timestamp_.IsInfinite()) {
    current_offset_ = feedback_receive_time;
  } else {
    // TODO(srte): We shouldn't need to do rounding here.
    const TimeDelta delta = feedback.GetBaseDelta(last_timestamp_)
                                .RoundDownTo(TimeDelta::Millis(1));
    // Protect against assigning current_offset_ negative value.
    if (delta < Timestamp::Zero() - current_offset_) {
      RTC_LOG(LS_WARNING) << "Unexpected feedback timestamp received.";
      current_offset_ = feedback_receive_time;
    } else {
      current_offset_ += delta;
    }
  }
  last_timestamp_ = feedback.BaseTime();

  std::vector<PacketResult> packet_result_vector;
  packet_result_vector.reserve(feedback.GetPacketStatusCount());

  int ignored = 0;

  feedback.ForAllPackets([&](uint16_t sequence_number,
                             TimeDelta delta_since_base) {
    int64_t seq_num = seq_num_unwrapper_.Unwrap(sequence_number);
    std::optional<PacketFeedback> packet_feedback = RetrievePacketFeedback(
        seq_num, /*received=*/delta_since_base.IsFinite());
    if (!packet_feedback) {
      return;
    }

    if (delta_since_base.IsFinite()) {
      packet_feedback->receive_time =
          current_offset_ + delta_since_base.RoundDownTo(TimeDelta::Millis(1));
    }
    if (packet_feedback->network_route == network_route_) {
      PacketResult result;
      result.sent_packet = packet_feedback->sent;
      result.receive_time = packet_feedback->receive_time;
      packet_result_vector.push_back(result);
    } else {
      ++ignored;
    }
  });

  if (ignored > 0) {
    RTC_LOG(LS_INFO) << "Ignoring " << ignored
                     << " packets because they were sent on a different route.";
  }

  return packet_result_vector;
}

std::optional<TransportPacketsFeedback>
TransportFeedbackAdapter::ProcessCongestionControlFeedback(
    const rtcp::CongestionControlFeedback& feedback,
    Timestamp feedback_receive_time) {
  if (feedback.packets().empty()) {
    RTC_LOG(LS_INFO) << "Empty transport layer feedback packet received.";
    return std::nullopt;
  }
  TransportPacketsFeedback msg;
  msg.feedback_time = feedback_receive_time;

  if (current_offset_.IsInfinite()) {
    current_offset_ =
        feedback_receive_time;  //.... todo - better use rtt/2 as offset...
  }
  TimeDelta feedback_delta =
      last_feedback_ntp_time_
          ? CompactNtpRttToTimeDelta(feedback.report_timestamp_compact_ntp() -
                                     *last_feedback_ntp_time_)

          : TimeDelta::Zero();
  RTC_LOG(LS_INFO) << "feedback_delta " << feedback_delta;
  last_feedback_ntp_time_ = feedback.report_timestamp_compact_ntp();

  if (feedback_delta < TimeDelta::Zero()) {
    RTC_LOG(LS_WARNING) << "Unexpected feedback ntp time delta "
                        << feedback_delta.ms() << " ms.";
    current_offset_ = feedback_receive_time;
  } else {
    current_offset_ += feedback_delta;
  }

  int64_t high_transport_sequence_number = -1;
  int64_t previous_highest_transport_sequence_number = last_ack_seq_num_;
  int ignored_packets = 0;
  for (const rtcp::CongestionControlFeedback::PacketInfo packet_info :
       feedback.packets()) {
    SsrcRtpSequenceNumberPair pair =
        std::make_pair(packet_info.ssrc, packet_info.sequence_number);
    auto it = rtp_to_transport_sequence_number_.find(pair);
    if (it == rtp_to_transport_sequence_number_.end()) {
      RTC_LOG(LS_WARNING) << " Transport sequence number not found for ssrc: "
                          << packet_info.ssrc
                          << "rtp sequence_no: " << packet_info.sequence_number;
      continue;
    }
    int64_t transport_sequence_number = it->second;
    if (transport_sequence_number > high_transport_sequence_number) {
      high_transport_sequence_number = transport_sequence_number;
    }
    std::optional<PacketFeedback> packet_feedback =
        RetrievePacketFeedback(transport_sequence_number, /*received*/ true);
    if (!packet_feedback) {
      RTC_DCHECK_NOTREACHED() << "Packetfeedback not found";
      continue;
    }
    if (packet_feedback->network_route == network_route_) {
      PacketResult result;
      result.sent_packet = packet_feedback->sent;
      RTC_DCHECK(packet_info.arrival_time_offset.IsFinite());
      result.receive_time = current_offset_ - packet_info.arrival_time_offset;
      result.ecn = packet_info.ecn;
      msg.packet_feedbacks.push_back(result);
    } else {
      ++ignored_packets;
    }
  }

  if (ignored_packets > 0) {
    RTC_LOG(LS_INFO) << "Ignoring " << ignored_packets
                     << " packets because they were sent on a different route.";
  }

  // Count all packets from `previous_highest_transport_sequence_number` to
  // high_transport_sequence_number that still exist in history as lost.
  for (int64_t tsn = previous_highest_transport_sequence_number + 1;
       tsn < high_transport_sequence_number; ++tsn) {
    auto history_it = history_.find(tsn);
    if (history_it != history_.end()) {
      // Packet still exist in history, so treet it as lost.
      PacketResult result;
      result.sent_packet = history_it->second.sent;
      msg.packet_feedbacks.push_back(result);
    }
  }
  // Feedback is expected to be sorted in send order.
  std::sort(msg.packet_feedbacks.begin(), msg.packet_feedbacks.end(),
            [](const PacketResult& lhs, const PacketResult& rhs) {
              return lhs.sent_packet.sequence_number <
                     rhs.sent_packet.sequence_number;
            });
  msg.data_in_flight = in_flight_.GetOutstandingData(network_route_);
  return msg;
}

}  // namespace webrtc
