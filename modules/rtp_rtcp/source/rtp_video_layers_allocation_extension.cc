/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"

#include "api/video/video_layers_allocation.h"
#include "rtc_base/bit_buffer.h"

namespace webrtc {

constexpr RTPExtensionType RtpVideoLayersAllocationExtension::kId;
constexpr const char RtpVideoLayersAllocationExtension::kUri[];
constexpr uint32_t kBpsPerKbps = 1000;

//  0                   1                   2
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | NS|Sid|T|X|Res| Bit encoded data...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// NS: Number of spatial layers/simulcast streams - 1. 2 bits, thus allowing
// passing number of layers/streams up-to 4.
// Sid: Simulcast stream id, numbered from 0. 2 bits.
// T: indicates if all spatial layers have the same amount of temporal layers.
// X: indicates if resolution and frame rate per spatial layer is present.
// Res: 2 bits reserved for future use.
// Bit encoded data: consists of following fields written in order:
//  1) T=1: Nt - 2-bit value of number of temporal layers - 1
//     T=0: NS 2-bit values of numbers of temporal layers - 1 for all spatial
//     layers from lower to higher.
//  2) Bitrates: One value for each spatial x temporal layer. First all bitrates
//     for the first spatial layer are written from the lower to higher temporal
//     layer, then for the second, etc.
//     All bitrates are in kbps, rounded up. If bitrate for some temporal layer
//     is written as 0, all higher temporal layers are implicitly assumed to
//     also be 0 and are skipped.
//     All bitrates are total required bitrate to receive the corresponding
//     layer, i.e. in simulcast mode they include only corresponding spatial
//     layer, in full-svc all lower spatial layers are included. All lower
//     temporal layers are also included.
//     All bitrates are written in one of the following formats:
//     0xxxxxxx - if the value fits in 7 bits,
//     10xxxxxx xxxxxxxx - if the value fits in 14 bits,
//     11xxxxxx xxxxxxxx xxxxxxxx - if the value fits in 22 bits
//     The maximum possible encoded value per temporal layer is a little more
//     than 4gbps
// 3) [only if X bit is set]. Encoded width and height 16-bit value followed by
//    max frame rate 8-bit per spatial layer in order from lower to higher.
// The extention can be as small as 3 bytes (1 spatial layer with low bitrate)
// as big as 69 bytes (4x4 layers with a very high bitrate and all data).

size_t RtpVideoLayersAllocationExtension::Encode(
    uint8_t* buffer,
    const VideoLayersAllocation& allocation,
    size_t buffer_size) {
  RTC_DCHECK_LT(allocation.simulcast_id, VideoLayersAllocation::kMaxSpatialIds);
  if (allocation.simulcast_id >= VideoLayersAllocation::kMaxSpatialIds)
    return 0;
  // nullptr buffer might be used to count size of the extension.
  // BitBufferWriter works with nullptr buffer as long as its size is 0.
  size_t bit_length = 0;
  RTC_CHECK(buffer != nullptr || buffer_size == 0);

  rtc::BitBufferWriter writer(buffer, buffer_size);

  // NS:
  int active_spatial_layers = 0;
  for (; active_spatial_layers < VideoLayersAllocation::kMaxSpatialIds;
       ++active_spatial_layers) {
    if (allocation.target_bitrate[active_spatial_layers].empty())
      break;
  }
  if (active_spatial_layers == 0)
    return 0;
  RTC_DCHECK(allocation.resolution_and_frame_rate.empty() ||
             active_spatial_layers ==
                 static_cast<int>(allocation.resolution_and_frame_rate.size()));
  bit_length += 2;
  writer.WriteBits(active_spatial_layers - 1, 2);

  // Sid:
  bit_length += 2;
  writer.WriteBits(allocation.simulcast_id, 2);

  // T:
  int sl_idx;
  bool num_tls_is_constant = true;
  for (sl_idx = 1; sl_idx < active_spatial_layers; ++sl_idx) {
    if (allocation.target_bitrate[sl_idx].size() !=
        allocation.target_bitrate[0].size()) {
      num_tls_is_constant = false;
    }
  }
  bit_length += 1;
  writer.WriteBits(num_tls_is_constant ? 1 : 0, 1);

  // X:
  bit_length += 1;
  bool has_full_data = !allocation.resolution_and_frame_rate.empty();
  writer.WriteBits(has_full_data ? 1 : 0, 1);

  // RES:
  bit_length += 2;
  writer.WriteBits(0, 2);

  if (num_tls_is_constant) {
    bit_length += 2;
    if (allocation.target_bitrate[0].size() >
        VideoLayersAllocation::kMaxTemporalIds)
      return 0;
    writer.WriteBits(allocation.target_bitrate[0].size() - 1, 2);
  } else {
    for (sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      bit_length += 2;
      if (allocation.target_bitrate[sl_idx].size() >
          VideoLayersAllocation::kMaxTemporalIds)
        return 0;
      writer.WriteBits(allocation.target_bitrate[sl_idx].size() - 1, 2);
    }
  }

  for (sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
    for (uint32_t bitrate : allocation.target_bitrate[sl_idx]) {
      bitrate /= kBpsPerKbps;
      if (bitrate < (1 << 7)) {
        bit_length += 8;
        writer.WriteBits(0, 1);
        writer.WriteBits(bitrate, 7);
      } else if (bitrate < (1 << 14)) {
        bit_length += 16;
        writer.WriteBits(0b10, 2);
        writer.WriteBits(bitrate, 14);
      } else if (bitrate < (1 << 22)) {
        bit_length += 24;
        writer.WriteBits(0b11, 2);
        writer.WriteBits(bitrate, 22);
      } else {
        return 0;
      }
      if (bitrate == 0)
        break;
    }
  }

  for (const auto& resolution : allocation.resolution_and_frame_rate) {
    writer.WriteUInt16(resolution.width);
    writer.WriteUInt16(resolution.height);
    writer.WriteUInt8(resolution.frame_rate);
  }

  return (bit_length + 7) / 8;
}

bool RtpVideoLayersAllocationExtension::Parse(
    rtc::ArrayView<const uint8_t> data,
    VideoLayersAllocation* allocation) {
  if (data.size() == 0)
    return false;
  rtc::BitBuffer reader(data.data(), data.size());
  if (!allocation)
    return false;

  uint32_t val;
  // NS:
  if (!reader.ReadBits(&val, 2))
    return false;
  int active_spatial_layers = val + 1;
  if (active_spatial_layers > VideoLayersAllocation::kMaxSpatialIds)
    return false;

  // Sid:
  if (!reader.ReadBits(&val, 2))
    return false;
  allocation->simulcast_id = val;
  if (allocation->simulcast_id >= VideoLayersAllocation::kMaxSpatialIds)
    return false;

  // T:
  int sl_idx, tl_idx;
  bool num_tls_is_constant = false;
  if (!reader.ReadBits(&val, 1))
    return false;
  num_tls_is_constant = val == 1;

  // X:
  if (!reader.ReadBits(&val, 1))
    return false;
  bool has_full_data = (val == 1);

  // RES:
  if (!reader.ReadBits(&val, 2))
    return false;

  int number_of_temporal_layers[VideoLayersAllocation::kMaxSpatialIds];
  if (num_tls_is_constant) {
    if (!reader.ReadBits(&val, 2))
      return false;
    if (val + 1 > VideoLayersAllocation::kMaxTemporalIds)
      return false;
    for (sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      number_of_temporal_layers[sl_idx] = val + 1;
    }
  } else {
    for (sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      if (!reader.ReadBits(&val, 2))
        return false;
      number_of_temporal_layers[sl_idx] = val + 1;
      if (number_of_temporal_layers[sl_idx] >
          VideoLayersAllocation::kMaxTemporalIds)
        return false;
    }
  }

  for (sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
    auto& temporal_layers = allocation->target_bitrate[sl_idx];
    temporal_layers.reserve(number_of_temporal_layers[sl_idx]);
    for (tl_idx = 0; tl_idx < number_of_temporal_layers[sl_idx]; ++tl_idx) {
      uint32_t bit;
      int bit_length = 0;
      if (!reader.ReadBits(&bit, 1))
        return false;
      if (bit == 0) {
        // 0xxxxxxx
        bit_length = 7;
        if (!reader.ReadBits(&val, 7))
          return false;
      } else {
        if (!reader.ReadBits(&bit, 1))
          return false;
        if (bit == 0) {
          // 10xxxxxx xxxxxxxx
          bit_length = 14;
        } else {
          // 11xxxxxx xxxxxxxx xxxxxxxx
          bit_length = 22;
        }
        if (!reader.ReadBits(&val, bit_length))
          return false;
      }
      allocation->target_bitrate[sl_idx].push_back(val * kBpsPerKbps);
      if (val == 0)
        break;
    }
  }

  if (has_full_data) {
    auto& resolutions = allocation->resolution_and_frame_rate;
    for (sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      resolutions.emplace_back();
      VideoLayersAllocation::ResolutionAndFrameRate& resolution =
          resolutions.back();
      if (!reader.ReadUInt16(&resolution.width))
        return false;
      if (!reader.ReadUInt16(&resolution.height))
        return false;
      if (!reader.ReadUInt8(&resolution.frame_rate))
        return false;
    }
  }
  return true;
}

size_t RtpVideoLayersAllocationExtension::ValueSize(
    const VideoLayersAllocation& allocation) {
  return Encode(nullptr, allocation, 0);
}

bool RtpVideoLayersAllocationExtension::Write(
    rtc::ArrayView<uint8_t> data,
    const VideoLayersAllocation& allocation) {
  return Encode(data.data(), allocation, data.size()) == data.size();
}

}  // namespace webrtc
