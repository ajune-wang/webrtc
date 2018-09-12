/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/instant_client.h"

#include <algorithm>

namespace webrtc {
namespace test {
namespace {
struct RawFeedbackReportPacket {
  static constexpr int MAX_FEEDBACKS = 10;
  struct Feedback {
    int16_t seq_offset;
    int32_t recv_offset_ms;
  };
  uint8_t count;
  int64_t first_seq_num;
  int64_t first_recv_time_ms;
  Feedback feedbacks[MAX_FEEDBACKS - 1];
};
}  // namespace

PacketStream::PacketStream(PacketStreamConfig config) : config_(config) {}

std::vector<int64_t> PacketStream::PullPackets(Timestamp at_time) {
  TimeDelta frame_interval = TimeDelta::seconds(1) / config_.frame_rate;
  if (next_frame_time_.IsInfinite()) {
    DataSize target_size = target_rate_ * frame_interval;
    DataSize packet_size = config_.initial_packet_size_multiplier * target_size;
    budget_ -= packet_size.bytes();
    next_frame_time_ = at_time + frame_interval;
    return {(packet_size + config_.packet_overhead).bytes()};
  }
  std::vector<int64_t> packets;
  while (at_time >= next_frame_time_) {
    next_frame_time_ += frame_interval;
    budget_ += (frame_interval * target_rate_).bytes();
    int64_t frame_budget = budget_ + (frame_interval * target_rate_).bytes();
    frame_budget = std::max(frame_budget, config_.min_frame_size.bytes());
    budget_ -= frame_budget;
    int64_t max_packet_size = config_.max_packet_size.bytes();

    while (frame_budget > max_packet_size) {
      packets.push_back(max_packet_size);
      frame_budget -= config_.max_packet_size.bytes();
    }
    packets.push_back(frame_budget);
  }
  return packets;
}

void PacketStream::OnTargetRateUpdate(DataRate target_rate) {
  target_rate_ = std::min(target_rate, config_.max_data_rate);
}

SimpleFeedbackReportPacket::SimpleFeedbackReportPacket() = default;

SimpleFeedbackReportPacket::SimpleFeedbackReportPacket(
    rtc::CopyOnWriteBuffer raw_packet) {
  RawFeedbackReportPacket report_packet;
  RTC_CHECK_LE(sizeof(report_packet), raw_packet.size());
  memcpy(&report_packet, raw_packet.cdata(), sizeof(report_packet));
  RTC_CHECK_GE(report_packet.count, 1);
  receive_times.emplace_back(report_packet.first_seq_num,
                             Timestamp::ms(report_packet.first_recv_time_ms));
  for (int i = 1; i < report_packet.count; ++i)
    receive_times.emplace_back(
        report_packet.first_seq_num + report_packet.feedbacks[i - 1].seq_offset,
        Timestamp::ms(report_packet.first_recv_time_ms +
                      report_packet.feedbacks[i - 1].recv_offset_ms));
}

rtc::CopyOnWriteBuffer SimpleFeedbackReportPacket::Build() const {
  RTC_CHECK_LE(receive_times.size(), RawFeedbackReportPacket::MAX_FEEDBACKS);
  RawFeedbackReportPacket report;
  report.count = receive_times.size();
  RTC_CHECK(!receive_times.empty());
  report.first_seq_num = receive_times.front().first;
  report.first_recv_time_ms = receive_times.front().second.ms();

  for (int i = 1; i < report.count; ++i) {
    report.feedbacks[i - 1].seq_offset =
        static_cast<int16_t>(receive_times[i].first - report.first_seq_num);
    report.feedbacks[i - 1].recv_offset_ms = static_cast<int32_t>(
        receive_times[i].second.ms() - report.first_recv_time_ms);
  }
  return rtc::CopyOnWriteBuffer(reinterpret_cast<uint8_t*>(&report),
                                sizeof(RawFeedbackReportPacket));
}

SimulatedSender::SimulatedSender(NetworkNode* send_node,
                                 uint64_t send_receiver_id)
    : send_node_(send_node), send_receiver_id_(send_receiver_id) {}

SimulatedSender::~SimulatedSender() {}

TransportPacketsFeedback SimulatedSender::PullFeedbackReport(
    SimpleFeedbackReportPacket packet,
    Timestamp at_time) {
  TransportPacketsFeedback report;
  report.prior_in_flight = data_in_flight_;
  report.feedback_time = at_time;

  for (auto& pair : packet.receive_times) {
    for (; next_feedback_seq_num_ <= pair.first; ++next_feedback_seq_num_) {
      PacketResult feedback;
      if (next_feedback_seq_num_ == pair.first)
        feedback.receive_time = pair.second;
      for (auto it = sent_packets_.begin(); it != sent_packets_.end(); it++) {
        if (it->sequence_number == next_feedback_seq_num_) {
          feedback.sent_packet = *it;
          if (feedback.receive_time.IsFinite())
            sent_packets_.erase(it);
          break;
        }
      }
      data_in_flight_ -= feedback.sent_packet->size;
      report.packet_feedbacks.push_back(feedback);
    }
  }
  report.data_in_flight = data_in_flight_;
  return report;
}

std::vector<SimulatedSender::PacketReadyToSend>
SimulatedSender::PullSendPackets(Timestamp at_time) {
  if (last_update_.IsInfinite()) {
    pacing_budget_ = 0;
  } else {
    TimeDelta delta = at_time - last_update_;
    pacing_budget_ += (delta * pacer_config_.data_rate()).bytes();
  }
  std::vector<PacketReadyToSend> to_send;
  while (data_in_flight_ <= max_in_flight_ && pacing_budget_ >= 0 &&
         !packet_queue_.empty()) {
    auto pending = packet_queue_.front();
    pacing_budget_ -= pending.size;
    packet_queue_.pop_front();
    SentPacket sent;
    sent.sequence_number = next_sequence_number_++;
    sent.size = DataSize::bytes(pending.size);
    data_in_flight_ += sent.size;
    sent.data_in_flight = data_in_flight_;
    sent.pacing_info = PacedPacketInfo();
    sent.send_time = at_time;
    sent_packets_.push_back(sent);

    rtc::CopyOnWriteBuffer packet(pending.size);
    memcpy(packet.data(), &sent.sequence_number, sizeof(sent.sequence_number));
    to_send.emplace_back(PacketReadyToSend{sent, packet});
  }
  pacing_budget_ = std::min(pacing_budget_, 0l);
  last_update_ = at_time;
  return to_send;
}

void SimulatedSender::Update(NetworkControlUpdate update) {
  if (update.pacer_config)
    pacer_config_ = *update.pacer_config;
  if (update.congestion_window)
    max_in_flight_ = *update.congestion_window;
}

SimulatedFeedback::SimulatedFeedback(InstantClientconfig config,
                                     uint64_t return_receiver_id,
                                     NetworkNode* return_node)
    : config_(config),
      return_receiver_id_(return_receiver_id),
      return_node_(return_node) {}

bool SimulatedFeedback::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                         uint64_t receiver,
                                         Timestamp at_time) {
  int64_t sequence_number;
  memcpy(&sequence_number, packet.cdata(), sizeof(sequence_number));
  receive_times_.insert({sequence_number, at_time});
  if (last_feedback_time_.IsInfinite())
    last_feedback_time_ = at_time;
  if (at_time >= last_feedback_time_ + config_.feedback.interval) {
    SimpleFeedbackReportPacket report;
    for (; next_feedback_seq_num_ <= sequence_number;
         ++next_feedback_seq_num_) {
      auto it = receive_times_.find(next_feedback_seq_num_);
      if (it != receive_times_.end()) {
        report.receive_times.emplace_back(next_feedback_seq_num_, it->second);
        receive_times_.erase(it);
      }
      if (receive_times_.size() >= RawFeedbackReportPacket::MAX_FEEDBACKS) {
        return_node_->TryDeliverPacket(report.Build(), return_receiver_id_,
                                       at_time);
        report = SimpleFeedbackReportPacket();
      }
    }
    if (!report.receive_times.empty())
      return_node_->TryDeliverPacket(report.Build(), return_receiver_id_,
                                     at_time);
    last_feedback_time_ = at_time;
  }
  return true;
}

InstantClient::InstantClient(std::string log_filename,
                             InstantClientconfig config,
                             std::vector<PacketStreamConfig> stream_configs,
                             std::vector<NetworkNode*> send_link,
                             std::vector<NetworkNode*> return_link,
                             uint64_t send_receiver_id,
                             uint64_t return_receiver_id,
                             Timestamp at_time)
    : network_controller_factory_(log_filename, config.transport),
      send_link_(send_link),
      return_link_(return_link),
      sender_(send_link.front(), send_receiver_id),
      feedback_(config, return_receiver_id, return_link.front()) {
  NetworkControllerConfig initial_config;
  initial_config.constraints.at_time = at_time;
  initial_config.constraints.starting_rate = config.transport.rates.start_rate;
  initial_config.constraints.min_data_rate = config.transport.rates.min_rate;
  initial_config.constraints.max_data_rate = config.transport.rates.max_rate;
  congestion_controller_ = network_controller_factory_.Create(initial_config);
  for (auto& stream_config : stream_configs)
    packet_streams_.emplace_back(new PacketStream(stream_config));
  NetworkNode::Route(send_receiver_id, &feedback_, send_link);
  NetworkNode::Route(return_receiver_id, this, return_link);

  CongestionProcess(at_time);
  network_controller_factory_.LogCongestionControllerStats(at_time);
  if (!log_filename.empty()) {
    std::string packet_log_name = log_filename + ".packets.txt";
    packet_log_ = fopen(packet_log_name.c_str(), "w");
    fprintf(packet_log_,
            "transport_seq packet_size send_time recv_time feed_time\n");
  }
}

bool InstantClient::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                     uint64_t receiver,
                                     Timestamp at_time) {
  auto report =
      sender_.PullFeedbackReport(SimpleFeedbackReportPacket(packet), at_time);
  for (PacketResult& feedback : report.packet_feedbacks) {
    if (packet_log_)
      fprintf(packet_log_, "%li %li %.3lf %.3lf %.3lf\n",
              feedback.sent_packet->sequence_number,
              feedback.sent_packet->size.bytes(),
              feedback.sent_packet->send_time.seconds<double>(),
              feedback.receive_time.seconds<double>(),
              at_time.seconds<double>());
  }
  Update(congestion_controller_->OnTransportPacketsFeedback(report));
  return true;
}
InstantClient::~InstantClient() {
  if (packet_log_)
    fclose(packet_log_);
}

void InstantClient::Update(NetworkControlUpdate update) {
  sender_.Update(update);
  if (update.target_rate) {
    double ratio_per_stream = 1.0 / packet_streams_.size();
    DataRate rate_per_stream =
        update.target_rate->target_rate * ratio_per_stream;
    target_rate_ = update.target_rate->target_rate;
    for (auto& stream : packet_streams_)
      stream->OnTargetRateUpdate(rate_per_stream);
  }
}

void InstantClient::CongestionProcess(Timestamp at_time) {
  ProcessInterval msg;
  msg.at_time = at_time;
  Update(congestion_controller_->OnProcessInterval(msg));
}

void InstantClient::PacerProcess(Timestamp at_time) {
  ProcessFrames(at_time);
  for (auto to_send : sender_.PullSendPackets(at_time)) {
    sender_.send_node_->TryDeliverPacket(to_send.data,
                                         sender_.send_receiver_id_, at_time);
    Update(congestion_controller_->OnSentPacket(to_send.send_info));
  }
}

void InstantClient::ProcessFrames(Timestamp at_time) {
  for (auto& stream : packet_streams_) {
    for (int64_t packet_size : stream->PullPackets(at_time)) {
      sender_.packet_queue_.push_back(
          SimulatedSender::PendingPacket{packet_size});
    }
  }
}

TimeDelta InstantClient::GetCongestionProcessInterval() const {
  return network_controller_factory_.GetProcessInterval();
}

double InstantClient::target_rate_kbps() const {
  return target_rate_.kbps<double>();
}

}  // namespace test
}  // namespace webrtc
