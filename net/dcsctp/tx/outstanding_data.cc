/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/outstanding_data.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "net/dcsctp/common/math.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "rtc_base/logging.h"

namespace dcsctp {

// The number of times a packet must be NACKed before it's retransmitted.
// See https://tools.ietf.org/html/rfc4960#section-7.2.4
constexpr size_t kNumberOfNacksForRetransmission = 3;

// Returns how large a chunk will be, serialized, carrying the data
size_t OutstandingData::GetSerializedChunkSize(const Data& data) const {
  return RoundUpTo4(data_chunk_header_size_ + data.size());
}

size_t OutstandingData::GetBoundedIndex(UnwrappedTSN tsn) const {
  if (tsn <= last_cumulative_tsn_ack_) {
    return 0;
  }
  size_t index = UnwrappedTSN::Difference(tsn, last_cumulative_tsn_ack_) - 1;
  return std::min(index, outstanding_data_.size());
}

void OutstandingData::Item::Ack() {
  ack_state_ = AckState::kAcked;
  should_be_retransmitted_ = false;
}

OutstandingData::Item::NackAction OutstandingData::Item::Nack(
    bool retransmit_now) {
  ack_state_ = AckState::kNacked;
  ++nack_count_;
  if ((retransmit_now || nack_count_ >= kNumberOfNacksForRetransmission) &&
      !is_abandoned_) {
    // Nacked enough times - it's considered lost.
    if (!max_retransmissions_.has_value() ||
        num_retransmissions_ < max_retransmissions_) {
      should_be_retransmitted_ = true;
      return NackAction::kRetransmit;
    }
    Abandon();
    return NackAction::kAbandon;
  }
  return NackAction::kNothing;
}

void OutstandingData::Item::Retransmit() {
  ack_state_ = AckState::kUnacked;
  should_be_retransmitted_ = false;

  nack_count_ = 0;
  ++num_retransmissions_;
}

void OutstandingData::Item::Abandon() {
  is_abandoned_ = true;
  should_be_retransmitted_ = false;
}

bool OutstandingData::Item::has_expired(TimeMs now) const {
  return expires_at_.has_value() && *expires_at_ < now;
}

bool OutstandingData::IsConsistent() const {
  size_t actual_outstanding_bytes = 0;
  size_t actual_outstanding_items = 0;

  std::set<UnwrappedTSN> actual_to_be_retransmitted;
  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (const auto& elem : outstanding_data_) {
    tsn.Increment();
    if (elem.is_outstanding()) {
      actual_outstanding_bytes += GetSerializedChunkSize(elem.data());
      ++actual_outstanding_items;
    }

    if (elem.should_be_retransmitted()) {
      actual_to_be_retransmitted.insert(tsn);
    }
  }

  return actual_outstanding_bytes == outstanding_bytes_ &&
         actual_outstanding_items == outstanding_items_ &&
         actual_to_be_retransmitted == to_be_retransmitted_;
}

void OutstandingData::AckChunk(AckInfo& ack_info,
                               UnwrappedTSN tsn,
                               Item& item) {
  if (!item.is_acked()) {
    size_t serialized_size = GetSerializedChunkSize(item.data());
    ack_info.bytes_acked += serialized_size;
    if (item.is_outstanding()) {
      outstanding_bytes_ -= serialized_size;
      --outstanding_items_;
    }
    if (item.should_be_retransmitted()) {
      to_be_retransmitted_.erase(tsn);
    }
    item.Ack();
    ack_info.highest_tsn_acked = std::max(ack_info.highest_tsn_acked, tsn);
  }
}

OutstandingData::AckInfo OutstandingData::HandleSack(
    UnwrappedTSN cumulative_tsn_ack,
    rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks,
    bool is_in_fast_retransmit) {
  OutstandingData::AckInfo ack_info(cumulative_tsn_ack);
  // Erase all items up to cumulative_tsn_ack.
  RemoveAcked(cumulative_tsn_ack, ack_info);

  // ACK packets reported in the gap ack blocks
  AckGapBlocks(cumulative_tsn_ack, gap_ack_blocks, ack_info);

  // NACK and possibly mark for retransmit chunks that weren't acked.
  NackBetweenAckBlocks(cumulative_tsn_ack, gap_ack_blocks,
                       is_in_fast_retransmit, ack_info);

  RTC_DCHECK(IsConsistent());
  return ack_info;
}

void OutstandingData::RemoveAcked(UnwrappedTSN cumulative_tsn_ack,
                                  AckInfo& ack_info) {
  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  std::deque<Item>::iterator it;
  for (it = outstanding_data_.begin(); it != outstanding_data_.end(); ++it) {
    tsn.Increment();
    if (tsn > cumulative_tsn_ack) {
      break;
    }
    AckChunk(ack_info, tsn, *it);
  }

  outstanding_data_.erase(outstanding_data_.begin(), it);
  last_cumulative_tsn_ack_ = cumulative_tsn_ack;
}

void OutstandingData::AckGapBlocks(
    UnwrappedTSN cumulative_tsn_ack,
    rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks,
    AckInfo& ack_info) {
  // Mark all non-gaps as ACKED (but they can't be removed) as (from RFC)
  // "SCTP considers the information carried in the Gap Ack Blocks in the
  // SACK chunk as advisory.". Note that when NR-SACK is supported, this can
  // be handled differently.

  for (auto& block : gap_ack_blocks) {
    UnwrappedTSN start_tsn =
        UnwrappedTSN::AddTo(cumulative_tsn_ack, block.start);
    UnwrappedTSN end_tsn =
        UnwrappedTSN::AddTo(cumulative_tsn_ack, block.end + 1);
    size_t start_idx = GetBoundedIndex(start_tsn);
    size_t end_idx = GetBoundedIndex(end_tsn);

    for (size_t idx = start_idx; idx < end_idx; ++idx) {
      UnwrappedTSN tsn = UnwrappedTSN::AddTo(last_cumulative_tsn_ack_, idx + 1);
      Item& item = outstanding_data_[idx];
      AckChunk(ack_info, tsn, item);
    }
  }
}

void OutstandingData::NackBetweenAckBlocks(
    UnwrappedTSN cumulative_tsn_ack,
    rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks,
    bool is_in_fast_recovery,
    OutstandingData::AckInfo& ack_info) {
  // Mark everything between the blocks as NACKED/TO_BE_RETRANSMITTED.
  // https://tools.ietf.org/html/rfc4960#section-7.2.4
  // "Mark the DATA chunk(s) with three miss indications for retransmission."
  // "For each incoming SACK, miss indications are incremented only for
  // missing TSNs prior to the highest TSN newly acknowledged in the SACK."
  //
  // What this means is that only when there is a increasing stream of data
  // received and there are new packets seen (since last time), packets that
  // are in-flight and between gaps should be nacked. This means that SCTP
  // relies on the T3-RTX-timer to re-send packets otherwise.
  UnwrappedTSN max_tsn_to_nack = ack_info.highest_tsn_acked;
  if (is_in_fast_recovery && cumulative_tsn_ack > last_cumulative_tsn_ack_) {
    // https://tools.ietf.org/html/rfc4960#section-7.2.4
    // "If an endpoint is in Fast Recovery and a SACK arrives that advances
    // the Cumulative TSN Ack Point, the miss indications are incremented for
    // all TSNs reported missing in the SACK."
    max_tsn_to_nack = UnwrappedTSN::AddTo(
        cumulative_tsn_ack,
        gap_ack_blocks.empty() ? 0 : gap_ack_blocks.rbegin()->end);
  }

  UnwrappedTSN prev_block_last_acked = cumulative_tsn_ack;
  for (auto& block : gap_ack_blocks) {
    UnwrappedTSN cur_block_first_acked =
        UnwrappedTSN::AddTo(cumulative_tsn_ack, block.start);
    for (size_t idx = GetBoundedIndex(prev_block_last_acked);
         idx < GetBoundedIndex(cur_block_first_acked); ++idx) {
      UnwrappedTSN tsn = UnwrappedTSN::AddTo(last_cumulative_tsn_ack_, idx + 1);

      if (tsn <= max_tsn_to_nack) {
        ack_info.has_packet_loss =
            NackItem(tsn, outstanding_data_[idx], /*retransmit_now=*/false);
      }
    }
    prev_block_last_acked =
        UnwrappedTSN::AddTo(cumulative_tsn_ack, block.end + 1);
  }

  // Note that packets are not NACKED which are above the highest
  // gap-ack-block (or above the cumulative ack TSN if no gap-ack-blocks) as
  // only packets up until the highest_tsn_acked (see above) should be
  // considered when NACKing.
}

bool OutstandingData::NackItem(UnwrappedTSN tsn,
                               Item& item,
                               bool retransmit_now) {
  if (item.is_outstanding()) {
    outstanding_bytes_ -= GetSerializedChunkSize(item.data());
    --outstanding_items_;
  }

  switch (item.Nack(retransmit_now)) {
    case Item::NackAction::kNothing:
      return false;
    case Item::NackAction::kRetransmit:
      to_be_retransmitted_.insert(tsn);
      RTC_DLOG(LS_VERBOSE) << *tsn.Wrap() << " marked for retransmission";
      break;
    case Item::NackAction::kAbandon:
      AbandonAllFor(item);
      break;
  }
  return true;
}

void OutstandingData::AbandonAllFor(const Item& item) {
  // Erase all remaining chunks from the producer, if any.
  if (discard_from_send_queue_(item.data().is_unordered, item.data().stream_id,
                               item.data().message_id)) {
    // There were remaining chunks to be produced for this message. Since the
    // receiver may have already received all chunks (up till now) for this
    // message, we can't just FORWARD-TSN to the last fragment in this
    // (abandoned) message and start sending a new message, as the receiver
    // will then see a new message before the end of the previous one was seen
    // (or skipped over). So create a new fragment, representing the end, that
    // the received will never see as it is abandoned immediately and used as
    // cum TSN in the sent FORWARD-TSN.
    UnwrappedTSN tsn = next_tsn();
    Data message_end(item.data().stream_id, item.data().ssn,
                     item.data().message_id, item.data().fsn, item.data().ppid,
                     std::vector<uint8_t>(), Data::IsBeginning(false),
                     Data::IsEnd(true), item.data().is_unordered);
    outstanding_data_.emplace_back(std::move(message_end), absl::nullopt,
                                   TimeMs(0), absl::nullopt);

    Item& added_item = outstanding_data_.back();
    // The added chunk shouldn't be included in `outstanding_bytes`, so set it
    // as acked.
    added_item.Ack();
    RTC_DLOG(LS_VERBOSE) << "Adding unsent end placeholder for message at tsn="
                         << *tsn.Wrap();
  }

  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (Item& other : outstanding_data_) {
    tsn.Increment();

    if (!other.is_abandoned() &&
        other.data().stream_id == item.data().stream_id &&
        other.data().is_unordered == item.data().is_unordered &&
        other.data().message_id == item.data().message_id) {
      RTC_DLOG(LS_VERBOSE) << "Marking chunk " << *tsn.Wrap()
                           << " as abandoned";
      if (other.should_be_retransmitted()) {
        to_be_retransmitted_.erase(tsn);
      }
      other.Abandon();
    }
  }
}

std::vector<std::pair<TSN, Data>> OutstandingData::GetChunksToBeRetransmitted(
    size_t max_size) {
  RTC_DCHECK(IsConsistent());
  std::vector<std::pair<TSN, Data>> result;

  for (auto it = to_be_retransmitted_.begin();
       it != to_be_retransmitted_.end();) {
    UnwrappedTSN tsn = *it;

    RTC_DCHECK(tsn > last_cumulative_tsn_ack_);
    RTC_DCHECK(tsn < next_tsn());
    size_t index = GetBoundedIndex(tsn);
    RTC_DCHECK(index != outstanding_data_.size());
    Item& item = outstanding_data_[index];
    RTC_DCHECK(item.should_be_retransmitted());
    RTC_DCHECK(!item.is_outstanding());
    RTC_DCHECK(!item.is_abandoned());
    RTC_DCHECK(!item.is_acked());

    size_t serialized_size = GetSerializedChunkSize(item.data());
    if (serialized_size <= max_size) {
      item.Retransmit();
      result.emplace_back(tsn.Wrap(), item.data().Clone());
      max_size -= serialized_size;
      outstanding_bytes_ += serialized_size;
      ++outstanding_items_;
      it = to_be_retransmitted_.erase(it);
    } else {
      ++it;
    }
    // No point in continuing if the packet is full.
    if (max_size <= data_chunk_header_size_) {
      break;
    }
  }

  RTC_DCHECK(IsConsistent());
  return result;
}

void OutstandingData::ExpireOutstandingChunks(TimeMs now) {
  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (const Item& item : outstanding_data_) {
    tsn.Increment();

    // Chunks that are nacked can be expired. Care should be taken not to
    // expire unacked (in-flight) chunks as they might have been received, but
    // the SACK is either delayed or in-flight and may be received later.
    if (item.is_abandoned()) {
      // Already abandoned.
    } else if (item.is_nacked() && item.has_expired(now)) {
      RTC_DLOG(LS_VERBOSE) << "Marking nacked chunk " << *tsn.Wrap()
                           << " and message " << *item.data().message_id
                           << " as expired";
      AbandonAllFor(item);
    } else {
      // A non-expired chunk. No need to iterate any further.
      break;
    }
  }
  RTC_DCHECK(IsConsistent());
}

UnwrappedTSN OutstandingData::highest_outstanding_tsn() const {
  return UnwrappedTSN::AddTo(last_cumulative_tsn_ack_,
                             outstanding_data_.size());
}

absl::optional<UnwrappedTSN> OutstandingData::Insert(
    const Data& data,
    absl::optional<size_t> max_retransmissions,
    TimeMs time_sent,
    absl::optional<TimeMs> expires_at) {
  UnwrappedTSN tsn = next_tsn();

  // All chunks are always padded to be even divisible by 4.
  size_t chunk_size = GetSerializedChunkSize(data);
  outstanding_bytes_ += chunk_size;
  ++outstanding_items_;
  outstanding_data_.emplace_back(data.Clone(), max_retransmissions, time_sent,
                                 expires_at);
  Item& item = outstanding_data_.back();

  if (item.has_expired(time_sent)) {
    // No need to send it - it was expired when it was in the send
    // queue.
    RTC_DLOG(LS_VERBOSE) << "Marking freshly produced chunk " << *tsn.Wrap()
                         << " and message " << *item.data().message_id
                         << " as expired";
    AbandonAllFor(item);
    RTC_DCHECK(IsConsistent());
    return absl::nullopt;
  }

  RTC_DCHECK(IsConsistent());
  return tsn;
}

void OutstandingData::NackAll() {
  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (Item& item : outstanding_data_) {
    tsn.Increment();
    if (!item.is_acked()) {
      NackItem(tsn, item, /*retransmit_now=*/true);
    }
  }
  RTC_DCHECK(IsConsistent());
}

absl::optional<DurationMs> OutstandingData::MeasureRTT(TimeMs now,
                                                       UnwrappedTSN tsn) const {
  if (tsn > last_cumulative_tsn_ack_ && tsn < next_tsn()) {
    size_t index = GetBoundedIndex(tsn);
    RTC_DCHECK(index != outstanding_data_.size());
    const Item& item = outstanding_data_[index];
    if (!item.has_been_retransmitted()) {
      // https://tools.ietf.org/html/rfc4960#section-6.3.1
      // "Karn's algorithm: RTT measurements MUST NOT be made using
      // packets that were retransmitted (and thus for which it is ambiguous
      // whether the reply was for the first instance of the chunk or for a
      // later instance)"
      return now - item.time_sent();
    }
  }
  return absl::nullopt;
}

std::vector<std::pair<TSN, OutstandingData::State>>
OutstandingData::GetChunkStatesForTesting() const {
  std::vector<std::pair<TSN, State>> states;
  states.emplace_back(last_cumulative_tsn_ack_.Wrap(), State::kAcked);
  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (const Item& item : outstanding_data_) {
    tsn.Increment();
    State state;
    if (item.is_abandoned()) {
      state = State::kAbandoned;
    } else if (item.should_be_retransmitted()) {
      state = State::kToBeRetransmitted;
    } else if (item.is_acked()) {
      state = State::kAcked;
    } else if (item.is_outstanding()) {
      state = State::kInFlight;
    } else {
      state = State::kNacked;
    }

    states.emplace_back(tsn.Wrap(), state);
  }
  return states;
}

bool OutstandingData::ShouldSendForwardTsn() const {
  if (!outstanding_data_.empty()) {
    return outstanding_data_.front().is_abandoned();
  }
  return false;
}

ForwardTsnChunk OutstandingData::CreateForwardTsn() const {
  std::map<StreamID, SSN> skipped_per_ordered_stream;
  UnwrappedTSN new_cumulative_ack = last_cumulative_tsn_ack_;

  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (const Item& item : outstanding_data_) {
    tsn.Increment();

    if (!item.is_abandoned()) {
      break;
    }
    new_cumulative_ack = tsn;
    if (!item.data().is_unordered &&
        item.data().ssn > skipped_per_ordered_stream[item.data().stream_id]) {
      skipped_per_ordered_stream[item.data().stream_id] = item.data().ssn;
    }
  }

  std::vector<ForwardTsnChunk::SkippedStream> skipped_streams;
  skipped_streams.reserve(skipped_per_ordered_stream.size());
  for (const auto& elem : skipped_per_ordered_stream) {
    skipped_streams.emplace_back(elem.first, elem.second);
  }
  return ForwardTsnChunk(new_cumulative_ack.Wrap(), std::move(skipped_streams));
}

IForwardTsnChunk OutstandingData::CreateIForwardTsn() const {
  std::map<std::pair<IsUnordered, StreamID>, MID> skipped_per_stream;
  UnwrappedTSN new_cumulative_ack = last_cumulative_tsn_ack_;

  UnwrappedTSN tsn = last_cumulative_tsn_ack_;
  for (const Item& item : outstanding_data_) {
    tsn.Increment();

    if (!item.is_abandoned()) {
      break;
    }
    new_cumulative_ack = tsn;
    std::pair<IsUnordered, StreamID> stream_id =
        std::make_pair(item.data().is_unordered, item.data().stream_id);

    if (item.data().message_id > skipped_per_stream[stream_id]) {
      skipped_per_stream[stream_id] = item.data().message_id;
    }
  }

  std::vector<IForwardTsnChunk::SkippedStream> skipped_streams;
  skipped_streams.reserve(skipped_per_stream.size());
  for (const auto& elem : skipped_per_stream) {
    const std::pair<IsUnordered, StreamID>& stream = elem.first;
    MID message_id = elem.second;
    skipped_streams.emplace_back(stream.first, stream.second, message_id);
  }

  return IForwardTsnChunk(new_cumulative_ack.Wrap(),
                          std::move(skipped_streams));
}

}  // namespace dcsctp
