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
#include <limits>
#include <utility>
#include <vector>

#include "absl/types/variant.h"
#include "api/array_view.h"
#include "api/rtp_packet_info.h"
#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/mod_ops.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace video_coding {

PacketBuffer::Packet::Packet(const RtpPacketReceived& rtp_packet,
                             const RTPVideoHeader& video_header,
                             int64_t ntp_time_ms,
                             int64_t receive_time_ms)
    : marker_bit(rtp_packet.Marker()),
      payload_type(rtp_packet.PayloadType()),
      seq_num(rtp_packet.SequenceNumber()),
      timestamp(rtp_packet.Timestamp()),
      ntp_time_ms(ntp_time_ms),
      times_nacked(-1),
      video_header(video_header),
      packet_info(rtp_packet.Ssrc(),
                  rtp_packet.Csrcs(),
                  rtp_packet.Timestamp(),
                  /*audio_level=*/absl::nullopt,
                  rtp_packet.GetExtension<AbsoluteCaptureTimeExtension>(),
                  receive_time_ms) {}

PacketBuffer::PacketBuffer(Clock* clock,
                           size_t start_buffer_size,
                           size_t max_buffer_size)
    : clock_(clock),
      max_size_(max_buffer_size),
      first_seq_num_(0),
      first_packet_received_(false),
      is_cleared_to_first_seq_num_(false),
      buffer_(start_buffer_size),
      sps_pps_idr_is_h264_keyframe_(false) {
  RTC_DCHECK_LE(start_buffer_size, max_buffer_size);
  // Buffer size must always be a power of 2.
  RTC_DCHECK((start_buffer_size & (start_buffer_size - 1)) == 0);
  RTC_DCHECK((max_buffer_size & (max_buffer_size - 1)) == 0);
}

// https://blog.csdn.net/sonysuqin/article/details/106629343
PacketBuffer::~PacketBuffer() {
  Clear();
}

PacketBuffer::InsertResult PacketBuffer::InsertPacket(
    std::unique_ptr<PacketBuffer::Packet> packet) {
  PacketBuffer::InsertResult result;
  MutexLock lock(&mutex_);

  // 当前包序列号
  uint16_t seq_num = packet->seq_num;
  // 当前包在包缓存(包括数据缓存和排序缓存)中的索引
  size_t index = seq_num % buffer_.size();

  // 如果是第一个包
  if (!first_packet_received_) {
    // 保存第一个包序列号
    first_seq_num_ = seq_num;
    // 接收到了第一个包后状态置位
    first_packet_received_ = true;
  } else if (AheadOf(first_seq_num_, seq_num)) { // 如果当前包比first_seq_num_还老
    // 并且之前已经清理过第一个包序列号，说明已经至少成功解码过一帧，RtpVideoStreamReceiver::FrameDecoded
    // 会调用PacketBuffer::ClearTo(seq_num)，清理first_seq_num_之前的所有缓存，
    // 这个时候还来一个比first_seq_num_还老的包，就没有必要再留着了

    // If we have explicitly cleared past this packet then it's old,
    // don't insert it, just silently ignore it.
    if (is_cleared_to_first_seq_num_) {
      return result;
    }

    // 相反如果没有被清理过，则是有必要保留成第一个包的，比如发生了乱序
    first_seq_num_ = seq_num;
  }

  // 如果这个槽被占用了
  if (buffer_[index] != nullptr) {
    // 如果序列号相等，则为重复包，删除负载并丢弃
    // Duplicate packet, just delete the payload.
    if (buffer_[index]->seq_num == packet->seq_num) {
      return result;
    }

    // 如果槽被占但是输入包和对应槽的包序列号不等，说明缓存满了，需要扩容
    // The packet buffer is full, try to expand the buffer.
    while (ExpandBufferSize() && buffer_[seq_num % buffer_.size()] != nullptr) {
    }
    // 重新计算输入包索引
    index = seq_num % buffer_.size();

    // 如果对应的槽还是被占用了，还是满，已经不行了，致命错误
    // Packet buffer is still full since we were unable to expand the buffer.
    if (buffer_[index] != nullptr) {
      // Clear the buffer, delete payload, and return false to signal that a
      // new keyframe is needed.
      RTC_LOG(LS_WARNING) << "Clear PacketBuffer and request key frame.";
      ClearInternal();
      result.buffer_cleared = true;
      return result;
    }
  }

  // 更新时间戳
  int64_t now_ms = clock_->TimeInMilliseconds();
  last_received_packet_ms_ = now_ms;
  if (packet->video_header.frame_type == VideoFrameType::kVideoFrameKey ||
      last_received_keyframe_rtp_timestamp_ == packet->timestamp) {
    last_received_keyframe_packet_ms_ = now_ms;
    last_received_keyframe_rtp_timestamp_ = packet->timestamp;
  }

  packet->continuous = false;
  buffer_[index] = std::move(packet);

  // 更新丢包信息，检查收到当前包后是否有丢包导致的空洞，也就是不连续
  UpdateMissingPackets(seq_num);

  // 分析排序缓存，检查是否能够组装出完整的帧并返回
  result.packets = FindFrames(seq_num);
  return result;
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
  MutexLock lock(&mutex_);
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
    auto& stored = buffer_[first_seq_num_ % buffer_.size()];
    if (stored != nullptr && AheadOf<uint16_t>(seq_num, stored->seq_num)) {
      stored = nullptr;
    }
    ++first_seq_num_;
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

void PacketBuffer::Clear() {
  MutexLock lock(&mutex_);
  ClearInternal();
}

// 发送端可能在编码器输出码率不足的情况下为保证发送码率填充空包，空包不会进入排序缓存和数据缓存，但是会触发丢包检测和完整帧的检测
PacketBuffer::InsertResult PacketBuffer::InsertPadding(uint16_t seq_num) {
  PacketBuffer::InsertResult result;
  MutexLock lock(&mutex_);
  // 更新丢包信息，检查收到当前包后是否有丢包导致的空洞，也就是不连续
  UpdateMissingPackets(seq_num);
  // 分析排序缓存，检查是否能够组装出完整的帧并返回
  result.packets = FindFrames(static_cast<uint16_t>(seq_num + 1));
  return result;
}

absl::optional<int64_t> PacketBuffer::LastReceivedPacketMs() const {
  MutexLock lock(&mutex_);
  return last_received_packet_ms_;
}

absl::optional<int64_t> PacketBuffer::LastReceivedKeyframePacketMs() const {
  MutexLock lock(&mutex_);
  return last_received_keyframe_packet_ms_;
}
void PacketBuffer::ForceSpsPpsIdrIsH264Keyframe() {
  sps_pps_idr_is_h264_keyframe_ = true;
}
void PacketBuffer::ClearInternal() {
  for (auto& entry : buffer_) {
    entry = nullptr;
  }

  first_packet_received_ = false;
  is_cleared_to_first_seq_num_ = false;
  last_received_packet_ms_.reset();
  last_received_keyframe_packet_ms_.reset();
  newest_inserted_seq_num_.reset();
  missing_packets_.clear();
}

bool PacketBuffer::ExpandBufferSize() {
  if (buffer_.size() == max_size_) {
    RTC_LOG(LS_WARNING) << "PacketBuffer is already at max size (" << max_size_
                        << "), failed to increase size.";
    return false;
  }

  size_t new_size = std::min(max_size_, 2 * buffer_.size());
  std::vector<std::unique_ptr<Packet>> new_buffer(new_size);
  for (std::unique_ptr<Packet>& entry : buffer_) {
    if (entry != nullptr) {
      new_buffer[entry->seq_num % new_size] = std::move(entry);
    }
  }
  buffer_ = std::move(new_buffer);
  RTC_LOG(LS_INFO) << "PacketBuffer size expanded to " << new_size;
  return true;
}

// 检测seq_num前的所有包是连续的，只有包连续才进入完整帧的检测，所以叫“潜在的新帧检测”
// 一个帧的第一个包当且仅当帧开始标识is_first_packet_in_frame才返回连续，而第二个包以后是否
// 返回连续依赖于上个包是否连续，这个连续性的延展保证只要判定某个序列号连续，其之前的所有包都连续
bool PacketBuffer::PotentialNewFrame(uint16_t seq_num) const {
  // 通过序列号获取缓存索引
  size_t index = seq_num % buffer_.size();
  // 上个包的索引
  int prev_index = index > 0 ? index - 1 : buffer_.size() - 1;
  const auto& entry = buffer_[index];
  const auto& prev_entry = buffer_[prev_index];

  // 如果当前包的槽位没有被占用，那么该包之前没有处理过，不连续
  if (entry == nullptr)
    return false;
  // 如果当前包的槽位的序列号和当前包序列号不一致，不连续
  if (entry->seq_num != seq_num)
    return false;
  // 如果当前包是帧第一个包，连续
  if (entry->is_first_packet_in_frame())
    return true;
  // 如果上个包的槽位没有被占用，那么上个包之前没有处理过，不连续
  if (prev_entry == nullptr)
    return false;
  // 如果上个包和当前包的序列号不连续，不连续
  if (prev_entry->seq_num != static_cast<uint16_t>(entry->seq_num - 1))
    return false;
  // 如果上个包的时间戳和当前包的时间戳不相等，不连续
  if (prev_entry->timestamp != entry->timestamp)
    return false;
  // 排除掉以上所有错误后，如果上个包连续，则可以认为当前包连续
  if (prev_entry->continuous)
    return true;

  // 如果上个包不连续或者有其他错误，就返回不连续
  return false;
}

// 帧完整性检查，如果得到完整帧，则通过上报。遍历排序缓存中连续的包，检查一帧的边界
// 对VPX，认为包的frame_begin可信，这样VPX的完整一帧就完全依赖于检测到frame_begin和frame_end这两个包；
// 对H264，认为包的frame_begin不可信，并不依赖frame_begin来判断帧的开始，但是frame_end仍然是可信的，
//        具体说H264的开始标识是通过从frame_end标识的一帧最后一个包向前追溯，直到找到一个时间戳不一样
//        的断层，认为找到了完整的一个H264的帧
// 基本算法：遍历所有连续包，先找到带有frame_end标识的帧最后一个包，然后向前回溯，
//         找到帧的第一个包(VPX是frame_begin, H264是时间戳不连续)，组成完整一帧
std::vector<std::unique_ptr<PacketBuffer::Packet>> PacketBuffer::FindFrames(
    uint16_t seq_num) {
  std::vector<std::unique_ptr<PacketBuffer::Packet>> found_frames;
  // PotentialNewFrame(seq_num)检测seq_num之前的所有包是否连续
  for (size_t i = 0; i < buffer_.size() && PotentialNewFrame(seq_num); ++i) {
    // 当前包的缓存索引
    size_t index = seq_num % buffer_.size();
    // 如果seq_num之前所有包连续，那么seq_num自己也连续
    buffer_[index]->continuous = true;

    // 找到了帧的最后一个包s
    // If all packets of the frame is continuous, find the first packet of the
    // frame and add all packets of the frame to the returned packets.
    if (buffer_[index]->is_last_packet_in_frame()) {
      // 帧开始序列号，从帧尾部开始
      uint16_t start_seq_num = seq_num;

      // 开始向前回溯，找帧的第一个包.帧开始的索引，从帧尾部开始
      // Find the start index by searching backward until the packet with
      // the |frame_begin| flag is set.
      int start_index = index;
      // 已经测试的包数
      size_t tested_packets = 0;
      // 当前包的时间戳
      int64_t frame_timestamp = buffer_[start_index]->timestamp;

      // Identify H.264 keyframes by means of SPS, PPS, and IDR.
      bool is_h264 = buffer_[start_index]->codec() == kVideoCodecH264;
      bool has_h264_sps = false;
      bool has_h264_pps = false;
      bool has_h264_idr = false;
      bool is_h264_keyframe = false;
      int idr_width = -1;
      int idr_height = -1;
      // 从帧尾部的包开始回溯
      while (true) {
        ++tested_packets;

        // 如果是VPX，并且找到了frame_begin标识的第一个包，一帧完整，回溯结束
        if (!is_h264 && buffer_[start_index]->is_first_packet_in_frame())
          break;

        if (is_h264) {
          const auto* h264_header = absl::get_if<RTPVideoHeaderH264>(
              &buffer_[start_index]->video_header.video_type_header);
          if (!h264_header || h264_header->nalus_length >= kMaxNalusPerPacket)
            return found_frames;

          // 遍历所有NALU，注意WebRTC所有IDR帧前面都会带SPS、PPS
          for (size_t j = 0; j < h264_header->nalus_length; ++j) {
            if (h264_header->nalus[j].type == H264::NaluType::kSps) {
              has_h264_sps = true;
            } else if (h264_header->nalus[j].type == H264::NaluType::kPps) {
              has_h264_pps = true;
            } else if (h264_header->nalus[j].type == H264::NaluType::kIdr) {
              has_h264_idr = true;
            }
          }
          // 默认sps_pps_idr_is_h264_keyframe_为false，就是说只需要有IDR帧就认为是关键帧，
          // 而不需要等待SPS、PPS完整
          if ((sps_pps_idr_is_h264_keyframe_ && has_h264_idr && has_h264_sps &&
               has_h264_pps) ||
              (!sps_pps_idr_is_h264_keyframe_ && has_h264_idr)) {
            is_h264_keyframe = true;
            // Store the resolution of key frame which is the packet with
            // smallest index and valid resolution; typically its IDR or SPS
            // packet; there may be packet preceeding this packet, IDR's
            // resolution will be applied to them.
            if (buffer_[start_index]->width() > 0 &&
                buffer_[start_index]->height() > 0) {
              idr_width = buffer_[start_index]->width();
              idr_height = buffer_[start_index]->height();
            }
          }
        }

        // 如果检测包数已经达到缓存容量，中止
        if (tested_packets == buffer_.size())
          break;

        // 搜索指针向前移动一个包
        start_index = start_index > 0 ? start_index - 1 : buffer_.size() - 1;

        // In the case of H264 we don't have a frame_begin bit (yes,
        // |frame_begin| might be set to true but that is a lie). So instead
        // we traverese backwards as long as we have a previous packet and
        // the timestamp of that packet is the same as this one. This may cause
        // the PacketBuffer to hand out incomplete frames.
        // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7106
                        // 如果该槽位未被占用，发现断层
        if (is_h264 && (buffer_[start_index] == nullptr ||
                        // 如果时间戳不一致，发现断层
                        buffer_[start_index]->timestamp != frame_timestamp)) {
          break; // 结束回溯
        }

        // 如果仍然在一帧内，开始包序列号--
        --start_seq_num;
      }

      // 到这里帧的开始和结束位置已经搜索完毕，可以开始组帧.
      // 但是对H264 P帧，需要做另外的特殊处理，虽然P帧可能已经完整，
      // 但是如果该P帧前面仍然有丢包空洞，不会立刻向后传递，会等待直到所有空洞被填满，
      // 因为P帧必须有参考帧才能正确解码
      if (is_h264) {
        // Warn if this is an unsafe frame.
        if (has_h264_idr && (!has_h264_sps || !has_h264_pps)) {
          RTC_LOG(LS_WARNING)
              << "Received H.264-IDR frame "
                 "(SPS: "
              << has_h264_sps << ", PPS: " << has_h264_pps << "). Treating as "
              << (sps_pps_idr_is_h264_keyframe_ ? "delta" : "key")
              << " frame since WebRTC-SpsPpsIdrIsH264Keyframe is "
              << (sps_pps_idr_is_h264_keyframe_ ? "enabled." : "disabled");
        }

        // Now that we have decided whether to treat this frame as a key frame
        // or delta frame in the frame buffer, we update the field that
        // determines if the RtpFrameObject is a key frame or delta frame.
        const size_t first_packet_index = start_seq_num % buffer_.size();
        if (is_h264_keyframe) {
          buffer_[first_packet_index]->video_header.frame_type =
              VideoFrameType::kVideoFrameKey;
          if (idr_width > 0 && idr_height > 0) {
            // IDR frame was finalized and we have the correct resolution for
            // IDR; update first packet to have same resolution as IDR.
            buffer_[first_packet_index]->video_header.width = idr_width;
            buffer_[first_packet_index]->video_header.height = idr_height;
          }
        } else {
          buffer_[first_packet_index]->video_header.frame_type =
              VideoFrameType::kVideoFrameDelta;
        }

        // missing_packets_.upper_bound(start_seq_num) != missing_packets_.begin()
        // 这个条件是说在丢包的列表里搜索>start_seq_num(帧开始序列号)的第一个位置，
        // 发现其不等于丢包列表的开头,有些丢的包序列号小于start_seq_num,也就是说P帧前面有丢包空洞
        // 举例1：
        // missing_packets_ = { 3, 4, 6 }, start_seq_num = 5, missing_packets_.upper_bound(start_seq_num)==6
        // 作为一帧开始位置的序列号5，前面还有3、4这两个包还未收到，那么对P帧来说，虽然完整，但是向后传递也可能是没有意义的
        // 举例2：
        // missing_packets_ = { 10, 16, 17 }, start_seq_num = 3, missing_packets_.upper_bound(start_seq_num)==10
        // 作为一帧开始位置的序列号3，前面并没有丢包，并且帧完整，那么可以向后传递

        // If this is not a keyframe, make sure there are no gaps in the packet
        // sequence numbers up until this point.
        if (!is_h264_keyframe && missing_packets_.upper_bound(start_seq_num) !=
                                     missing_packets_.begin()) {
          return found_frames;
        }
      }

      const uint16_t end_seq_num = seq_num + 1;
      // Use uint16_t type to handle sequence number wrap around case.
      uint16_t num_packets = end_seq_num - start_seq_num;
      found_frames.reserve(found_frames.size() + num_packets);
      for (uint16_t i = start_seq_num; i != end_seq_num; ++i) {
        std::unique_ptr<Packet>& packet = buffer_[i % buffer_.size()];
        RTC_DCHECK(packet);
        RTC_DCHECK_EQ(i, packet->seq_num);
        // Ensure frame boundary flags are properly set.
        packet->video_header.is_first_packet_in_frame = (i == start_seq_num);
        packet->video_header.is_last_packet_in_frame = (i == seq_num);
        found_frames.push_back(std::move(packet));
      }

      // 马上要组帧了，清除丢包列表中到帧开始位置之前的丢包.
      // 对H264 P帧来说，如果P帧前面有空洞不会运行到这里，上面已经解释.
      //        I帧来说，可以丢弃前面的丢包信息
      missing_packets_.erase(missing_packets_.begin(),
                             missing_packets_.upper_bound(seq_num));
    }
    // 向后扩大搜索的范围，假设丢包、乱序，当前包的seq_num刚好填补了之前的一个空洞，
    // 该包并不能检测出一个完整帧，需要这里向后移动指针到frame_end再进行回溯，直到检测出完整帧，
    // 这里会继续检测之前缓存的因为前面有空洞而没有向后传递的P帧
    ++seq_num;
  }
  return found_frames;
}

// 更新丢包信息，用于检查P帧前面的空洞
// 丢包缓存missing_packets_，主要用于在PacketBuffer::FindFrames中判断某个已经完整的P帧
// 前面是否有未完整的帧，如果有，该帧可能是I帧，也可能是P帧，这里并不会立刻把这个完整的P帧向后
// 传递给RtpFrameReferenceFinder，而是暂时清除状态，等待前面的所有帧完整后才重复检测操作，
// 所以这里实际上也发生了帧的排序，并产生了一定的帧间依赖
void PacketBuffer::UpdateMissingPackets(uint16_t seq_num) {
  // 如果最新插入的包序列号还未设置过，这里直接设置一次
  if (!newest_inserted_seq_num_)
    newest_inserted_seq_num_ = seq_num;

  const int kMaxPaddingAge = 1000;
  // 如果当前包的序列号新于之前的最新包序列号，没有发生乱序
  if (AheadOf(seq_num, *newest_inserted_seq_num_)) {
    /* ............ newest_inserted_seq_num_ ........... < seq_num
     *        |                              ---> |
     * (1)    |                              old_seq_num(erase_to)
     * (2) old_seq_num(erase_to)
     */

    // 丢包缓存missing_packets_最大保存1000个包，这里得到当前包1000个包以前的序列号，
    // 也就差不多是丢包缓存里应该保存的最老的包
    uint16_t old_seq_num = seq_num - kMaxPaddingAge;
    // 第一个>=old_seq_num的包的位置
    auto erase_to = missing_packets_.lower_bound(old_seq_num);
    // 删除丢包缓存里所有1000个包之前的所有包(如果有的话)
    missing_packets_.erase(missing_packets_.begin(), erase_to);

    // (1) 如果最老的包的序列号都比当前最新包序列号新，那么更新一下当前最新包序列号
    // Guard against inserting a large amount of missing packets if there is a
    // jump in the sequence number.
    if (AheadOf(old_seq_num, *newest_inserted_seq_num_))
      *newest_inserted_seq_num_ = old_seq_num;

    // 因为seq_num > newest_inserted_seq_num_，这里开始统计(newest_inserted_seq_num_, sum)之间的空洞
    ++*newest_inserted_seq_num_;
    // 从newest_inserted_seq_num_开始，每个小于当前seq_num的包都进入丢包缓存，
    // 直到newest_inserted_seq_num_ == seq_num，也就是最新包的序列号变成了当前seq_num
    while (AheadOf(seq_num, *newest_inserted_seq_num_)) {
      missing_packets_.insert(*newest_inserted_seq_num_);
      ++*newest_inserted_seq_num_;
    }
  } else {
    // 如果当前收到的包的序列号小于当前收到的最新包序列号，则从丢包缓存中删除(之前应该已经进入丢包缓存)
    missing_packets_.erase(seq_num);
  }
}

}  // namespace video_coding
}  // namespace webrtc
