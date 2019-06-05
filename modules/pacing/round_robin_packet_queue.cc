/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/round_robin_packet_queue.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {
int GetPriorityForType(RtpPacketPacer::PacketType type) {
  switch (type) {
    case RtpPacketPacer::PacketType::kAudio:
      // Audio is always prioritized over other packet types.
      return 0;
    case RtpPacketPacer::PacketType::kRetransmission:
      // Send retransmissions before new media.
      return 1;
    case RtpPacketPacer::PacketType::kVideo:
      // Video has "normal" priority, in the old speak.
      return 2;
    case RtpPacketPacer::PacketType::kRedundancy:
      // Redundancy is OK to drop, but the content is hopefully not useless.
      return 3;
    case RtpPacketPacer::PacketType::kPadding:
      // Packets that are in themselves likely useless, only sent to keep the
      // BWE high.
      return 4;
  }
}
}  // namespace

RoundRobinPacketQueue::PacketInfo::PacketInfo(const PacketInfo& rhs) = default;
RoundRobinPacketQueue::PacketInfo::~PacketInfo() = default;

RoundRobinPacketQueue::PacketInfo::PacketInfo(
    RtpPacketPacer::PacketType type,
    uint32_t ssrc,
    uint16_t seq_number,
    int64_t capture_time_ms,
    int64_t enqueue_time_ms,
    size_t length_in_bytes,
    bool retransmission,
    uint64_t enqueue_order,
    std::multiset<int64_t>::iterator enqueue_time_it,
    absl::optional<RtpPacketIterator> packet_it)
    : type_(type),
      priority_(GetPriorityForType(type)),
      ssrc_(ssrc),
      sequence_number_(seq_number),
      capture_time_ms_(capture_time_ms),
      enqueue_time_ms_(enqueue_time_ms),
      bytes_(length_in_bytes),
      retransmission_(retransmission),
      enqueue_order_(enqueue_order),
      enqueue_time_it_(enqueue_time_it),
      packet_it_(packet_it) {}

std::unique_ptr<RtpPacketToSend>
RoundRobinPacketQueue::PacketInfo::ReleasePacket() {
  return packet_it_ ? std::move(**packet_it_) : nullptr;
}

void RoundRobinPacketQueue::PacketInfo::SubtractPauseTimeMs(
    int64_t pause_time_sum_ms) {
  enqueue_time_ms_ -= pause_time_sum_ms;
}

bool RoundRobinPacketQueue::PacketInfo::operator<(
    const RoundRobinPacketQueue::PacketInfo& other) const {
  if (priority_ != other.priority_)
    return priority_ > other.priority_;
  if (retransmission_ != other.retransmission_)
    return other.retransmission_;

  return enqueue_order_ > other.enqueue_order_;
}

RoundRobinPacketQueue::Stream::Stream() : bytes(0), ssrc(0) {}
RoundRobinPacketQueue::Stream::Stream(const Stream& stream) = default;
RoundRobinPacketQueue::Stream::~Stream() {}

RoundRobinPacketQueue::RoundRobinPacketQueue(int64_t start_time_us)
    : time_last_updated_ms_(start_time_us / 1000) {}

RoundRobinPacketQueue::~RoundRobinPacketQueue() {}

void RoundRobinPacketQueue::Push(RtpPacketPacer::PacketType type,
                                 uint32_t ssrc,
                                 uint16_t seq_number,
                                 int64_t capture_time_ms,
                                 int64_t enqueue_time_ms,
                                 size_t length_in_bytes,
                                 bool retransmission,
                                 uint64_t enqueue_order) {
  Push(PacketInfo(type, ssrc, seq_number, capture_time_ms, enqueue_time_ms,
                  length_in_bytes, retransmission, enqueue_order,
                  enqueue_times_.insert(enqueue_time_ms), absl::nullopt));
}

void RoundRobinPacketQueue::Push(RtpPacketPacer::PacketType type,
                                 int64_t enqueue_time_ms,
                                 uint64_t enqueue_order,
                                 std::unique_ptr<RtpPacketToSend> packet) {
  rtp_packets_.push_front(std::move(packet));
  Push(PacketInfo(
      type, packet->Ssrc(), packet->SequenceNumber(), packet->capture_time_ms(),
      enqueue_time_ms, packet->payload_size(),
      type == RtpPacketPacer::PacketType::kRetransmission, enqueue_order,
      enqueue_times_.insert(enqueue_time_ms), rtp_packets_.begin()));
}

RoundRobinPacketQueue::PacketInfo* RoundRobinPacketQueue::BeginPop() {
  RTC_CHECK(!pop_packet_ && !pop_stream_);

  Stream* stream = GetHighestPriorityStream();
  pop_stream_.emplace(stream);
  pop_packet_.emplace(stream->packet_queue.top());
  stream->packet_queue.pop();

  return &pop_packet_.value();
}

void RoundRobinPacketQueue::CancelPop() {
  RTC_CHECK(pop_packet_ && pop_stream_);
  (*pop_stream_)->packet_queue.push(*pop_packet_);
  pop_packet_.reset();
  pop_stream_.reset();
}

void RoundRobinPacketQueue::FinalizePop() {
  if (!Empty()) {
    RTC_CHECK(pop_packet_ && pop_stream_);
    Stream* stream = *pop_stream_;
    stream_priorities_.erase(stream->priority_it);
    const PacketInfo& packet = *pop_packet_;

    // Calculate the total amount of time spent by this packet in the queue
    // while in a non-paused state. Note that the |pause_time_sum_ms_| was
    // subtracted from |packet.enqueue_time_ms| when the packet was pushed, and
    // by subtracting it now we effectively remove the time spent in in the
    // queue while in a paused state.
    int64_t time_in_non_paused_state_ms =
        time_last_updated_ms_ - packet.EnqueueTimeMs() - pause_time_sum_ms_;
    queue_time_sum_ms_ -= time_in_non_paused_state_ms;

    RTC_CHECK(packet.EnqueueTimeIterator() != enqueue_times_.end());
    enqueue_times_.erase(packet.EnqueueTimeIterator());

    auto packet_it = packet.PacketIterator();
    if (packet_it) {
      rtp_packets_.erase(*packet_it);
    }

    // Update |bytes| of this stream. The general idea is that the stream that
    // has sent the least amount of bytes should have the highest priority.
    // The problem with that is if streams send with different rates, in which
    // case a "budget" will be built up for the stream sending at the lower
    // rate. To avoid building a too large budget we limit |bytes| to be within
    // kMaxLeading bytes of the stream that has sent the most amount of bytes.
    stream->bytes = std::max(stream->bytes + packet.SizeInBytes(),
                             max_bytes_ - kMaxLeadingBytes);
    max_bytes_ = std::max(max_bytes_, stream->bytes);

    size_bytes_ -= packet.SizeInBytes();
    size_packets_ -= 1;
    RTC_CHECK(size_packets_ > 0 || queue_time_sum_ms_ == 0);

    // If there are packets left to be sent, schedule the stream again.
    RTC_CHECK(!IsSsrcScheduled(stream->ssrc));
    if (stream->packet_queue.empty()) {
      stream->priority_it = stream_priorities_.end();
    } else {
      int priority = stream->packet_queue.top().Priority();
      stream->priority_it = stream_priorities_.emplace(
          StreamPrioKey(priority, stream->bytes), stream->ssrc);
    }

    pop_packet_.reset();
    pop_stream_.reset();
  }
}

bool RoundRobinPacketQueue::Empty() const {
  RTC_CHECK((!stream_priorities_.empty() && size_packets_ > 0) ||
            (stream_priorities_.empty() && size_packets_ == 0));
  return stream_priorities_.empty();
}

size_t RoundRobinPacketQueue::SizeInPackets() const {
  return size_packets_;
}

uint64_t RoundRobinPacketQueue::SizeInBytes() const {
  return size_bytes_;
}

int64_t RoundRobinPacketQueue::OldestEnqueueTimeMs() const {
  if (Empty())
    return 0;
  RTC_CHECK(!enqueue_times_.empty());
  return *enqueue_times_.begin();
}

void RoundRobinPacketQueue::UpdateQueueTime(int64_t timestamp_ms) {
  RTC_CHECK_GE(timestamp_ms, time_last_updated_ms_);
  if (timestamp_ms == time_last_updated_ms_)
    return;

  int64_t delta_ms = timestamp_ms - time_last_updated_ms_;

  if (paused_) {
    pause_time_sum_ms_ += delta_ms;
  } else {
    queue_time_sum_ms_ += delta_ms * size_packets_;
  }

  time_last_updated_ms_ = timestamp_ms;
}

void RoundRobinPacketQueue::SetPauseState(bool paused, int64_t timestamp_ms) {
  if (paused_ == paused)
    return;
  UpdateQueueTime(timestamp_ms);
  paused_ = paused;
}

int64_t RoundRobinPacketQueue::AverageQueueTimeMs() const {
  if (Empty())
    return 0;
  return queue_time_sum_ms_ / size_packets_;
}

void RoundRobinPacketQueue::Push(PacketInfo packet) {
  auto stream_info_it = streams_.find(packet.Ssrc());
  if (stream_info_it == streams_.end()) {
    stream_info_it = streams_.emplace(packet.Ssrc(), Stream()).first;
    stream_info_it->second.priority_it = stream_priorities_.end();
    stream_info_it->second.ssrc = packet.Ssrc();
  }

  Stream* stream = &stream_info_it->second;

  if (stream->priority_it == stream_priorities_.end()) {
    // If the SSRC is not currently scheduled, add it to |stream_priorities_|.
    RTC_CHECK(!IsSsrcScheduled(stream->ssrc));
    stream->priority_it = stream_priorities_.emplace(
        StreamPrioKey(packet.Priority(), stream->bytes), packet.Ssrc());
  } else if (packet.Priority() < stream->priority_it->first.priority) {
    // If the priority of this SSRC increased, remove the outdated StreamPrioKey
    // and insert a new one with the new priority. Note that |priority_| uses
    // lower ordinal for higher priority.
    stream_priorities_.erase(stream->priority_it);
    stream->priority_it = stream_priorities_.emplace(
        StreamPrioKey(packet.Priority(), stream->bytes), packet.Ssrc());
  }
  RTC_CHECK(stream->priority_it != stream_priorities_.end());

  // In order to figure out how much time a packet has spent in the queue while
  // not in a paused state, we subtract the total amount of time the queue has
  // been paused so far, and when the packet is popped we subtract the total
  // amount of time the queue has been paused at that moment. This way we
  // subtract the total amount of time the packet has spent in the queue while
  // in a paused state.
  UpdateQueueTime(packet.EnqueueTimeMs());
  packet.SubtractPauseTimeMs(pause_time_sum_ms_);

  size_packets_ += 1;
  size_bytes_ += packet.SizeInBytes();

  stream->packet_queue.push(packet);
}

RoundRobinPacketQueue::Stream*
RoundRobinPacketQueue::GetHighestPriorityStream() {
  RTC_CHECK(!stream_priorities_.empty());
  uint32_t ssrc = stream_priorities_.begin()->second;

  auto stream_info_it = streams_.find(ssrc);
  RTC_CHECK(stream_info_it != streams_.end());
  RTC_CHECK(stream_info_it->second.priority_it == stream_priorities_.begin());
  RTC_CHECK(!stream_info_it->second.packet_queue.empty());
  return &stream_info_it->second;
}

bool RoundRobinPacketQueue::IsSsrcScheduled(uint32_t ssrc) const {
  for (const auto& scheduled_stream : stream_priorities_) {
    if (scheduled_stream.second == ssrc)
      return true;
  }
  return false;
}

}  // namespace webrtc
