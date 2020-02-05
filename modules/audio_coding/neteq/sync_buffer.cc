/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/sync_buffer.h"

#include <algorithm>  // Access to min.

#include "rtc_base/checks.h"

namespace webrtc {

size_t SyncBuffer::FutureLength() const {
  return Size() - next_index_;
}

void SyncBuffer::PopFront(size_t length) {
  AudioMultiVector::PopFront(length);
  // Update the index of all rtp packet infos and discard old ones.
  auto first_valid_info = packet_infos_.begin();
  for (auto it = packet_infos_.rbegin(); it != packet_infos_.rend(); ++it) {
    if (it->first <= length) {
      // Erase until previous element (excluded).
      first_valid_info = it != packet_infos_.rbegin() ? std::prev(it).base()
                                                      : packet_infos_.end();
      break;
    }
    // Move forward.
    it->first -= length;
  }
  if (first_valid_info != packet_infos_.begin())
    packet_infos_.erase(packet_infos_.begin(), first_valid_info);
}

void SyncBuffer::PopBack(size_t length) {
  AudioMultiVector::PopBack(length);
  // Delete poped ones.
  size_t size = Size();
  packet_infos_.erase(
      std::remove_if(packet_infos_.begin(), packet_infos_.end(),
                     [size](const auto& info) { return info.first >= size; }),
      packet_infos_.end());
}

void SyncBuffer::PushBack(const AudioMultiVector& append_this,
                          const RtpPacketInfos& packet_infos) {
  PushBack(append_this);
  packet_infos_.emplace_back(next_index_, packet_infos);
}

void SyncBuffer::PushBack(const AudioMultiVector& append_this) {
  size_t samples_added = append_this.Size();
  AudioMultiVector::PushBack(append_this);
  PopFront(samples_added);
  if (samples_added <= next_index_) {
    next_index_ -= samples_added;
  } else {
    // This means that we are pushing out future data that was never used.
    //    assert(false);
    // TODO(hlundin): This assert must be disabled to support 60 ms frames.
    // This should not happen even for 60 ms frames, but it does. Investigate
    // why.
    next_index_ = 0;
  }
  dtmf_index_ -= std::min(dtmf_index_, samples_added);
}

void SyncBuffer::PushBackInterleaved(const rtc::BufferT<int16_t>& append_this) {
  const size_t size_before_adding = Size();
  AudioMultiVector::PushBackInterleaved(append_this);
  const size_t samples_added_per_channel = Size() - size_before_adding;
  RTC_DCHECK_EQ(samples_added_per_channel * Channels(), append_this.size());
  PopFront(samples_added_per_channel);
  next_index_ -= std::min(next_index_, samples_added_per_channel);
  dtmf_index_ -= std::min(dtmf_index_, samples_added_per_channel);
}

void SyncBuffer::PushFrontZeros(size_t length) {
  InsertZerosAtIndex(length, 0);
}

void SyncBuffer::InsertZerosAtIndex(size_t length, size_t position) {
  position = std::min(position, Size());
  length = std::min(length, Size() - position);
  PopBack(length);
  for (size_t channel = 0; channel < Channels(); ++channel) {
    channels_[channel]->InsertZerosAt(length, position);
  }
  // Update the index of all rtp packet infos.
  for (auto& info : packet_infos_) {
    if (info.first >= position) {
      info.first += length;
    }
  }
  if (next_index_ >= position) {
    // We are moving the |next_index_| sample.
    set_next_index(next_index_ + length);  // Overflow handled by subfunction.
  }
  if (dtmf_index_ > 0 && dtmf_index_ >= position) {
    // We are moving the |dtmf_index_| sample.
    set_dtmf_index(dtmf_index_ + length);  // Overflow handled by subfunction.
  }
}

void SyncBuffer::ReplaceAtIndex(const AudioMultiVector& insert_this,
                                size_t length,
                                size_t position,
                                const RtpPacketInfos& packet_infos) {
  position = std::min(position, Size());  // Cap |position| in the valid range.
  length = std::min(length, Size() - position);
  AudioMultiVector::OverwriteAt(insert_this, length, position);
  // Delete replaced ones.
  auto insert_at = packet_infos_.erase(
      std::remove_if(packet_infos_.begin(), packet_infos_.end(),
                     [=](const auto& info) {
                       return info.first >= position &&
                              info.first < position + length;
                     }),
      packet_infos_.end());
  // Insert new info in order.
  packet_infos_.emplace(insert_at, position, packet_infos);
}

void SyncBuffer::ReplaceAtIndex(const AudioMultiVector& insert_this,
                                size_t position,
                                const RtpPacketInfos& packet_infos) {
  ReplaceAtIndex(insert_this, insert_this.Size(), position, packet_infos);
}

size_t SyncBuffer::ReadInterleavedWithInfo(size_t length,
                                           int16_t* destination,
                                           RtpPacketInfos& packet_infos) const {
  return ReadInterleavedFromIndexWithInfo(0, length, destination, packet_infos);
}

size_t SyncBuffer::ReadInterleavedFromEndWithInfo(
    size_t length,
    int16_t* destination,
    RtpPacketInfos& packet_infos) const {
  length = std::min(length, Size());  // Cannot read more than Size() elements.
  return ReadInterleavedFromIndexWithInfo(Size() - length, length, destination,
                                          packet_infos);
}

size_t SyncBuffer::ReadInterleavedFromIndexWithInfo(
    size_t start_index,
    size_t length,
    int16_t* destination,
    RtpPacketInfos& packet_infos) const {
  std::vector<RtpPacketInfo> frame_packet_infos;
  for (const auto& info : packet_infos_) {
    if (info.first >= start_index && info.first < start_index + length) {
      frame_packet_infos.insert(packet_infos.end(), info.second.begin(),
                                info.second.end());
    } else if (info.first >= start_index + length) {
      break;
    }
  }
  packet_infos = RtpPacketInfos(std::move(frame_packet_infos));
  return AudioMultiVector::ReadInterleavedFromIndex(start_index, length,
                                                    destination);
}

void SyncBuffer::GetNextAudioInterleaved(size_t requested_len,
                                         AudioFrame* output) {
  const size_t samples_to_read = std::min(FutureLength(), requested_len);
  output->ResetWithoutMuting();
  std::vector<RtpPacketInfo> frame_packet_infos;
  // Update the index of all rtp packet infos and erase read ones.
  auto it = packet_infos_.begin();
  while (it != packet_infos_.end()) {
    if (it->first < requested_len) {
      // Move RtpPacketInfos from RtpPacketInfos to vector.
      std::move(it->second.begin(), it->second.end(),
                std::back_inserter(frame_packet_infos));
      it = packet_infos_.erase(it);
    } else {
      it->first -= requested_len;
      ++it;
    }
  }
  const size_t tot_samples_read = ReadInterleavedFromIndex(
      next_index_, samples_to_read, output->mutable_data());
  const size_t samples_read_per_channel = tot_samples_read / Channels();
  next_index_ += samples_read_per_channel;
  output->num_channels_ = Channels();
  output->samples_per_channel_ = samples_read_per_channel;
  output->packet_infos_ = RtpPacketInfos(std::move(frame_packet_infos));
}

void SyncBuffer::IncreaseEndTimestamp(uint32_t increment) {
  end_timestamp_ += increment;
}

void SyncBuffer::Flush() {
  Zeros(Size());
  next_index_ = Size();
  end_timestamp_ = 0;
  dtmf_index_ = 0;
  packet_infos_.clear();
}

void SyncBuffer::set_next_index(size_t value) {
  // Cannot set |next_index_| larger than the size of the buffer.
  next_index_ = std::min(value, Size());
}

void SyncBuffer::set_dtmf_index(size_t value) {
  // Cannot set |dtmf_index_| larger than the size of the buffer.
  dtmf_index_ = std::min(value, Size());
}
}  // namespace webrtc
