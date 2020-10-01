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

namespace {
constexpr uint32_t kBpsPerKbps = 1000;
}

namespace webrtc {

constexpr RTPExtensionType RtpVideoLayersAllocationExtension::kId;
constexpr const char RtpVideoLayersAllocationExtension::kUri[];

//  0                   1                   2
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | NS|Sid|T|X|Res| Bit encoded data...
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// NS: Number of spatial layers/simulcast streams - 1. 2 bits, thus allowing
// passing number of layers/streams up-to 4.
// Sid: RTP stream id, numbered from 0. 2 bits.
// T: indicates if all spatial layers have the same amount of temporal layers.
// X: indicates if resolution and frame rate per spatial layer is present.
// Res: 2 bits reserved for future use.
// Bit encoded data: consists of following fields written in order:
//  1) T=1: Nt - 2-bit value of number of temporal layers - 1
//     T=0: NS 2-bit values of numbers of temporal layers - 1 for all spatial
//     layers from lower to higher.
//  2) [only if X bit is set]. Encoded width, 16-bit, height, 16-bit,
//    max frame rate 8-bit per spatial layer in order from lower to higher.
//  3) Bitrates: One value for each spatial x temporal layer. First all bitrates
//     for the first spatial layer are written from the lower to higher temporal
//     layer, then for the second, etc.
//     All bitrates are in kbps, rounded up.
//     All bitrates are total required bitrate to receive the corresponding
//     layer, i.e. in simulcast mode they include only corresponding spatial
//     layer, in full-svc all lower spatial layers are included. All lower
//     temporal layers are also included.
//     All bitrates are written using unsigned Exponential Golomb encoding.

bool RtpVideoLayersAllocationExtension::Write(
    rtc::ArrayView<uint8_t> data,
    const VideoLayersAllocation& allocation) {
  RTC_DCHECK_LT(allocation.rtp_stream_index,
                VideoLayersAllocation::kMaxSpatialIds);
  RTC_DCHECK_GE(data.size(), ValueSize(allocation));

  rtc::BitBufferWriter writer(data.data(), data.size());

  // NS:
  int active_spatial_layers = 0;
  for (; active_spatial_layers < VideoLayersAllocation::kMaxSpatialIds;
       ++active_spatial_layers) {
    if (allocation.target_bitrate[active_spatial_layers].empty())
      break;
  }
  if (active_spatial_layers == 0)
    return false;
  RTC_DCHECK(allocation.resolution_and_frame_rate.empty() ||
             active_spatial_layers ==
                 static_cast<int>(allocation.resolution_and_frame_rate.size()));
  writer.WriteBits(active_spatial_layers - 1, 2);

  // Sid:
  writer.WriteBits(allocation.rtp_stream_index, 2);

  // T:
  bool num_tls_is_the_same = true;
  for (int sl_idx = 1; sl_idx < active_spatial_layers; ++sl_idx) {
    if (allocation.target_bitrate[sl_idx].size() !=
        allocation.target_bitrate[0].size()) {
      num_tls_is_the_same = false;
      break;
    }
  }
  writer.WriteBits(num_tls_is_the_same ? 1 : 0, 1);

  // X:
  writer.WriteBits(allocation.resolution_and_frame_rate.empty() ? 0 : 1, 1);

  // RESERVED:
  writer.WriteBits(/*val=*/0, /*bit_count=*/2);

  if (num_tls_is_the_same) {
    if (allocation.target_bitrate[0].size() >
        VideoLayersAllocation::kMaxTemporalIds) {
      return false;
    }
    writer.WriteBits(allocation.target_bitrate[0].size() - 1, 2);
  } else {
    for (int sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      if (allocation.target_bitrate[sl_idx].size() >
          VideoLayersAllocation::kMaxTemporalIds) {
        return false;
      }
      writer.WriteBits(allocation.target_bitrate[sl_idx].size() - 1, 2);
    }
  }

  for (const auto& resolution : allocation.resolution_and_frame_rate) {
    writer.WriteUInt16(resolution.width);
    writer.WriteUInt16(resolution.height);
    writer.WriteUInt8(resolution.frame_rate);
  }

  for (int sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
    for (uint32_t bitrate : allocation.target_bitrate[sl_idx]) {
      bitrate = 1 + ((bitrate - 1) / kBpsPerKbps);
      writer.WriteExponentialGolomb(bitrate);
    }
  }
  return true;
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

  // Sid:
  if (!reader.ReadBits(&val, 2))
    return false;
  allocation->rtp_stream_index = val;

  // T:
  if (!reader.ReadBits(&val, 1))
    return false;
  bool num_tls_is_constant = (val == 1);

  // X:
  if (!reader.ReadBits(&val, 1))
    return false;
  bool has_full_data = (val == 1);

  // RESERVED:
  if (!reader.ReadBits(&val, 2))
    return false;

  int number_of_temporal_layers[VideoLayersAllocation::kMaxSpatialIds];
  if (num_tls_is_constant) {
    if (!reader.ReadBits(&val, 2))
      return false;
    for (int sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      number_of_temporal_layers[sl_idx] = val + 1;
    }
  } else {
    for (int sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
      if (!reader.ReadBits(&val, 2))
        return false;
      number_of_temporal_layers[sl_idx] = val + 1;
      if (number_of_temporal_layers[sl_idx] >
          VideoLayersAllocation::kMaxTemporalIds)
        return false;
    }
  }

  if (has_full_data) {
    auto& resolutions = allocation->resolution_and_frame_rate;
    for (int sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
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

  for (int sl_idx = 0; sl_idx < active_spatial_layers; ++sl_idx) {
    auto& temporal_layers = allocation->target_bitrate[sl_idx];
    temporal_layers.reserve(number_of_temporal_layers[sl_idx]);
    for (int tl_idx = 0; tl_idx < number_of_temporal_layers[sl_idx]; ++tl_idx) {
      reader.ReadExponentialGolomb(&val);
      allocation->target_bitrate[sl_idx].push_back(val * kBpsPerKbps);
      if (val == 0)
        break;
    }
  }
  return true;
}

size_t RtpVideoLayersAllocationExtension::ValueSize(
    const VideoLayersAllocation& allocation) {
  size_t size = 1;  // Fixed first byte.¨
  for (int sid = 0; sid < VideoLayersAllocation::kMaxSpatialIds; ++sid) {
    for (uint32_t bitrate : allocation.target_bitrate[sid]) {
      size += rtc::BitBufferWriter::SizeExponentialGolomb(bitrate);
    }
  }
  size += allocation.resolution_and_frame_rate.size() *
          sizeof(VideoLayersAllocation::ResolutionAndFrameRate) * 8;
  return size;
}

}  // namespace webrtc
