/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/packet_buffer.h"

#include <string.h>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "absl/types/variant.h"
#include "api/video/encoded_frame.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/frame_object.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/mod_ops.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace video_coding {

void PacketBuffer::StoredPacket::SetUsed(VCMPacket* packet) {
  RTC_DCHECK(!used());
  continuous_ = false;
  frame_created_ = false;
  data_ = *packet;
  packet->dataPtr = nullptr;
  RTC_DCHECK(used());
}

void PacketBuffer::StoredPacket::SetUnused() {
  delete[] data_.dataPtr;
  data_.dataPtr = nullptr;
  RTC_DCHECK(!used());
}

PacketBuffer::PacketBuffer(Clock* clock,
                           size_t start_buffer_size,
                           size_t max_buffer_size,
                           OnAssembledFrameCallback* assembled_frame_callback)
    : clock_(clock),
      max_size_(max_buffer_size),
      first_seq_num_(0),
      first_packet_received_(false),
      is_cleared_to_first_seq_num_(false),
      buffer_(start_buffer_size),
      assembled_frame_callback_(assembled_frame_callback),
      unique_frames_seen_(0),
      sps_pps_idr_is_h264_keyframe_(
          field_trial::IsEnabled("WebRTC-SpsPpsIdrIsH264Keyframe")) {
  RTC_DCHECK_LE(start_buffer_size, max_buffer_size);
  // Buffer size must always be a power of 2.
  RTC_DCHECK((start_buffer_size & (start_buffer_size - 1)) == 0);
  RTC_DCHECK((max_buffer_size & (max_buffer_size - 1)) == 0);
}

PacketBuffer::~PacketBuffer() {
  Clear();
}

bool PacketBuffer::InsertPacket(VCMPacket* packet) {
  RTC_DCHECK(packet->dataPtr != nullptr);
  std::vector<std::unique_ptr<RtpFrameObject>> found_frames;
  {
    rtc::CritScope lock(&crit_);

    OnTimestampReceived(packet->timestamp);

    uint16_t seq_num = packet->seqNum;
    size_t index = seq_num % buffer_.size();

    if (!first_packet_received_) {
      first_seq_num_ = seq_num;
      first_packet_received_ = true;
    } else if (AheadOf(first_seq_num_, seq_num)) {
      // If we have explicitly cleared past this packet then it's old,
      // don't insert it, just silently ignore it.
      if (is_cleared_to_first_seq_num_) {
        delete[] packet->dataPtr;
        packet->dataPtr = nullptr;
        return true;
      }

      first_seq_num_ = seq_num;
    }

    if (buffer_[index].used()) {
      // Duplicate packet, just delete the payload.
      if (buffer_[index].seq_num() == seq_num) {
        delete[] packet->dataPtr;
        packet->dataPtr = nullptr;
        return true;
      }

      // The packet buffer is full, try to expand the buffer.
      while (ExpandBufferSize() && buffer_[seq_num % buffer_.size()].used()) {
      }
      index = seq_num % buffer_.size();

      // Packet buffer is still full since we were unable to expand the buffer.
      if (buffer_[index].used()) {
        // Clear the buffer, delete payload, and return false to signal that a
        // new keyframe is needed.
        RTC_LOG(LS_WARNING) << "Clear PacketBuffer and request key frame.";
        Clear();
        delete[] packet->dataPtr;
        packet->dataPtr = nullptr;
        return false;
      }
    }

    buffer_[index].SetUsed(packet);

    UpdateMissingPackets(packet->seqNum);

    int64_t now_ms = clock_->TimeInMilliseconds();
    last_received_packet_ms_ = now_ms;
    if (packet->video_header.frame_type == VideoFrameType::kVideoFrameKey)
      last_received_keyframe_packet_ms_ = now_ms;

    found_frames = FindFrames(seq_num);
  }

  for (std::unique_ptr<RtpFrameObject>& frame : found_frames)
    assembled_frame_callback_->OnAssembledFrame(std::move(frame));

  return true;
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  // We have already cleared past this sequence number, no need to do anything.
  if (is_cleared_to_first_seq_num_ &&
      AheadOf<uint16_t>(first_seq_num_, seq_num)) {
    return;
  }

  // If the packet buffer was cleared between a frame was created and returned.
  if (!first_packet_received_)
    return;

  // Avoid iterating over the buffer more than once by capping the number of
  // iterations to the |size_| of the buffer.
  ++seq_num;
  size_t diff = ForwardDiff<uint16_t>(first_seq_num_, seq_num);
  size_t iterations = std::min(diff, buffer_.size());
  for (size_t i = 0; i < iterations; ++i) {
    size_t index = (first_seq_num_ + i) % buffer_.size();
    if (AheadOf<uint16_t>(seq_num, buffer_[index].seq_num())) {
      buffer_[index].SetUnused();
    }
  }

  // If |diff| is larger than |iterations| it means that we don't increment
  // |first_seq_num_| until we reach |seq_num|, so we set it here.
  first_seq_num_ = seq_num;

  is_cleared_to_first_seq_num_ = true;
  auto clear_to_it = missing_packets_.upper_bound(seq_num);
  if (clear_to_it != missing_packets_.begin()) {
    --clear_to_it;
    missing_packets_.erase(missing_packets_.begin(), clear_to_it);
  }
}

void PacketBuffer::ClearInterval(uint16_t start_seq_num,
                                 uint16_t stop_seq_num) {
  size_t iterations = ForwardDiff<uint16_t>(start_seq_num, stop_seq_num + 1);
  RTC_DCHECK_LE(iterations, buffer_.size());
  uint16_t seq_num = start_seq_num;
  for (size_t i = 0; i < iterations; ++i) {
    size_t index = seq_num % buffer_.size();
    RTC_DCHECK_EQ(buffer_[index].seq_num(), seq_num);
    buffer_[index].SetUnused();
    ++seq_num;
  }
}

void PacketBuffer::Clear() {
  rtc::CritScope lock(&crit_);
  for (StoredPacket& entry : buffer_) {
    entry.SetUnused();
  }

  first_packet_received_ = false;
  is_cleared_to_first_seq_num_ = false;
  last_received_packet_ms_.reset();
  last_received_keyframe_packet_ms_.reset();
  newest_inserted_seq_num_.reset();
  missing_packets_.clear();
}

void PacketBuffer::PaddingReceived(uint16_t seq_num) {
  std::vector<std::unique_ptr<RtpFrameObject>> found_frames;
  {
    rtc::CritScope lock(&crit_);
    UpdateMissingPackets(seq_num);
    found_frames = FindFrames(static_cast<uint16_t>(seq_num + 1));
  }

  for (std::unique_ptr<RtpFrameObject>& frame : found_frames)
    assembled_frame_callback_->OnAssembledFrame(std::move(frame));
}

absl::optional<int64_t> PacketBuffer::LastReceivedPacketMs() const {
  rtc::CritScope lock(&crit_);
  return last_received_packet_ms_;
}

absl::optional<int64_t> PacketBuffer::LastReceivedKeyframePacketMs() const {
  rtc::CritScope lock(&crit_);
  return last_received_keyframe_packet_ms_;
}

int PacketBuffer::GetUniqueFramesSeen() const {
  rtc::CritScope lock(&crit_);
  return unique_frames_seen_;
}

bool PacketBuffer::ExpandBufferSize() {
  if (buffer_.size() == max_size_) {
    RTC_LOG(LS_WARNING) << "PacketBuffer is already at max size (" << max_size_
                        << "), failed to increase size.";
    return false;
  }

  size_t new_size = std::min(max_size_, 2 * buffer_.size());
  decltype(buffer_) new_buffer(new_size);
  for (StoredPacket& entry : buffer_) {
    if (entry.used()) {
      new_buffer[entry.seq_num() % new_size] = std::move(entry);
    }
  }
  buffer_ = std::move(new_buffer);
  RTC_LOG(LS_INFO) << "PacketBuffer size expanded to " << new_size;
  return true;
}

bool PacketBuffer::PotentialNewFrame(uint16_t seq_num) const {
  const StoredPacket* entry = GetPacket(seq_num);
  const StoredPacket* prev_entry = GetPacket(seq_num - 1);

  if (entry == nullptr || entry->frame_created())
    return false;
  if (entry->frame_begin())
    return true;
  if (prev_entry == nullptr || prev_entry->frame_created())
    return false;
  if (prev_entry->Timestamp() != entry->Timestamp())
    return false;
  if (prev_entry->continuous())
    return true;

  return false;
}

std::vector<std::unique_ptr<RtpFrameObject>> PacketBuffer::FindFrames(
    uint16_t seq_num) {
  std::vector<std::unique_ptr<RtpFrameObject>> found_frames;
  for (size_t i = 0; i < buffer_.size() && PotentialNewFrame(seq_num); ++i) {
    size_t index = seq_num % buffer_.size();
    buffer_[index].SetContinuous();

    // If all packets of the frame is continuous, find the first packet of the
    // frame and create an RtpFrameObject.
    if (buffer_[index].frame_end()) {
      size_t frame_size = 0;
      int max_nack_count = -1;
      uint16_t start_seq_num = seq_num;
      int64_t min_recv_time = buffer_[index].receive_time_ms();
      int64_t max_recv_time = buffer_[index].receive_time_ms();
      RtpPacketInfos::vector_type packet_infos;

      // Find the start index by searching backward until the packet with
      // the |frame_begin| flag is set.
      int start_index = index;
      size_t tested_packets = 0;
      int64_t frame_timestamp = buffer_[start_index].Timestamp();

      // Identify H.264 keyframes by means of SPS, PPS, and IDR.
      bool is_h264 = buffer_[start_index].codec() == kVideoCodecH264;
      bool has_h264_sps = false;
      bool has_h264_pps = false;
      bool has_h264_idr = false;
      bool is_h264_keyframe = false;

      while (true) {
        ++tested_packets;
        frame_size += buffer_[start_index].bitstream().size();
        max_nack_count =
            std::max(max_nack_count, buffer_[start_index].times_nacked());
        buffer_[start_index].SetFrameCreated(true);

        min_recv_time =
            std::min(min_recv_time, buffer_[start_index].receive_time_ms());
        max_recv_time =
            std::max(max_recv_time, buffer_[start_index].receive_time_ms());

        // Should use |push_front()| since the loop traverses backwards. But
        // it's too inefficient to do so on a vector so we'll instead fix the
        // order afterwards.
        packet_infos.push_back(buffer_[start_index].PacketInfo());

        if (!is_h264 && buffer_[start_index].frame_begin())
          break;

        if (is_h264) {
          const RTPVideoHeaderH264* h264_header =
              buffer_[start_index].h264_header();
          if (!h264_header || h264_header->nalus_length >= kMaxNalusPerPacket)
            return found_frames;

          for (size_t j = 0; j < h264_header->nalus_length; ++j) {
            if (h264_header->nalus[j].type == H264::NaluType::kSps) {
              has_h264_sps = true;
            } else if (h264_header->nalus[j].type == H264::NaluType::kPps) {
              has_h264_pps = true;
            } else if (h264_header->nalus[j].type == H264::NaluType::kIdr) {
              has_h264_idr = true;
            }
          }
          if ((sps_pps_idr_is_h264_keyframe_ && has_h264_idr && has_h264_sps &&
               has_h264_pps) ||
              (!sps_pps_idr_is_h264_keyframe_ && has_h264_idr)) {
            is_h264_keyframe = true;
          }
        }

        if (tested_packets == buffer_.size())
          break;

        start_index = start_index > 0 ? start_index - 1 : buffer_.size() - 1;

        // In the case of H264 we don't have a frame_begin bit (yes,
        // |frame_begin| might be set to true but that is a lie). So instead
        // we traverese backwards as long as we have a previous packet and
        // the timestamp of that packet is the same as this one. This may cause
        // the PacketBuffer to hand out incomplete frames.
        // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7106
        if (is_h264 && (!buffer_[start_index].used() ||
                        buffer_[start_index].Timestamp() != frame_timestamp)) {
          break;
        }

        --start_seq_num;
      }

      // Fix the order since the packet-finding loop traverses backwards.
      std::reverse(packet_infos.begin(), packet_infos.end());

      if (is_h264) {
        // Warn if this is an unsafe frame.
        if (has_h264_idr && (!has_h264_sps || !has_h264_pps)) {
          RTC_LOG(LS_WARNING)
              << "Received H.264-IDR frame "
              << "(SPS: " << has_h264_sps << ", PPS: " << has_h264_pps
              << "). Treating as "
              << (sps_pps_idr_is_h264_keyframe_ ? "delta" : "key")
              << " frame since WebRTC-SpsPpsIdrIsH264Keyframe is "
              << (sps_pps_idr_is_h264_keyframe_ ? "enabled." : "disabled");
        }

        // Now that we have decided whether to treat this frame as a key frame
        // or delta frame in the frame buffer, we update the field that
        // determines if the RtpFrameObject is a key frame or delta frame.
        const size_t first_packet_index = start_seq_num % buffer_.size();
        buffer_[first_packet_index].SetIsKeyframe(is_h264_keyframe);

        // With IPPP, if this is not a keyframe, make sure there are no gaps
        // in the packet sequence numbers up until this point.
        const uint8_t h264tid = buffer_[start_index].H264TemporalId();
        if (h264tid == kNoTemporalIdx && !is_h264_keyframe &&
            missing_packets_.upper_bound(start_seq_num) !=
                missing_packets_.begin()) {
          uint16_t stop_index = (index + 1) % buffer_.size();
          while (start_index != stop_index) {
            buffer_[start_index].SetFrameCreated(false);
            start_index = (start_index + 1) % buffer_.size();
          }

          return found_frames;
        }
      }

      missing_packets_.erase(missing_packets_.begin(),
                             missing_packets_.upper_bound(seq_num));

      const StoredPacket* first_packet = GetPacket(start_seq_num);
      const StoredPacket* last_packet = GetPacket(seq_num);
      auto frame = std::make_unique<RtpFrameObject>(
          start_seq_num, seq_num, last_packet->Marker(), max_nack_count,
          min_recv_time, max_recv_time, first_packet->Timestamp(),
          first_packet->ntp_time_ms(), last_packet->video_header().video_timing,
          first_packet->PayloadType(), first_packet->codec(),
          last_packet->video_header().rotation,
          last_packet->video_header().content_type,
          first_packet->video_header(), last_packet->video_header().color_space,
          first_packet->generic_descriptor(),
          RtpPacketInfos(std::move(packet_infos)),
          GetEncodedImageBuffer(frame_size, start_seq_num, seq_num));

      found_frames.emplace_back(std::move(frame));

      ClearInterval(start_seq_num, seq_num);
    }
    ++seq_num;
  }
  return found_frames;
}

rtc::scoped_refptr<EncodedImageBuffer> PacketBuffer::GetEncodedImageBuffer(
    size_t frame_size,
    uint16_t first_seq_num,
    uint16_t last_seq_num) {
  size_t index = first_seq_num % buffer_.size();
  size_t end = (last_seq_num + 1) % buffer_.size();

  auto buffer = EncodedImageBuffer::Create(frame_size);
  size_t offset = 0;

  do {
    RTC_DCHECK(buffer_[index].used());

    rtc::ArrayView<const uint8_t> payload = buffer_[index].bitstream();
    RTC_CHECK_LE(offset + payload.size(), payload.size());
    memcpy(buffer->data() + offset, payload.data(), payload.size());
    offset += payload.size();

    index = (index + 1) % buffer_.size();
  } while (index != end);

  return buffer;
}

const PacketBuffer::StoredPacket* PacketBuffer::GetPacket(
    uint16_t seq_num) const {
  const StoredPacket& entry = buffer_[seq_num % buffer_.size()];
  if (entry.used() && seq_num == entry.seq_num()) {
    return &entry;
  }
  return nullptr;
}

void PacketBuffer::UpdateMissingPackets(uint16_t seq_num) {
  if (!newest_inserted_seq_num_)
    newest_inserted_seq_num_ = seq_num;

  const int kMaxPaddingAge = 1000;
  if (AheadOf(seq_num, *newest_inserted_seq_num_)) {
    uint16_t old_seq_num = seq_num - kMaxPaddingAge;
    auto erase_to = missing_packets_.lower_bound(old_seq_num);
    missing_packets_.erase(missing_packets_.begin(), erase_to);

    // Guard against inserting a large amount of missing packets if there is a
    // jump in the sequence number.
    if (AheadOf(old_seq_num, *newest_inserted_seq_num_))
      *newest_inserted_seq_num_ = old_seq_num;

    ++*newest_inserted_seq_num_;
    while (AheadOf(seq_num, *newest_inserted_seq_num_)) {
      missing_packets_.insert(*newest_inserted_seq_num_);
      ++*newest_inserted_seq_num_;
    }
  } else {
    missing_packets_.erase(seq_num);
  }
}

void PacketBuffer::OnTimestampReceived(uint32_t rtp_timestamp) {
  const size_t kMaxTimestampsHistory = 1000;
  if (rtp_timestamps_history_set_.insert(rtp_timestamp).second) {
    rtp_timestamps_history_queue_.push(rtp_timestamp);
    ++unique_frames_seen_;
    if (rtp_timestamps_history_set_.size() > kMaxTimestampsHistory) {
      uint32_t discarded_timestamp = rtp_timestamps_history_queue_.front();
      rtp_timestamps_history_set_.erase(discarded_timestamp);
      rtp_timestamps_history_queue_.pop();
    }
  }
}

}  // namespace video_coding
}  // namespace webrtc
