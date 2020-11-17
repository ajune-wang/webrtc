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

#include <stddef.h>
#include <stdint.h>

#include "absl/algorithm/container.h"
#include "api/video/video_layers_allocation.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"

namespace webrtc {

constexpr RTPExtensionType RtpVideoLayersAllocationExtension::kId;
constexpr const char RtpVideoLayersAllocationExtension::kUri[];

namespace {

constexpr int kMaxNumRtpStreams = 4;

struct SpatialLayersBitmasks {
  int max_rtp_stream_id = 0;
  uint8_t spatial_layer_bitmask[kMaxNumRtpStreams] = {};
  bool are_the_same = true;
};

SpatialLayersBitmasks SpatialLayersBitmasksPerRtpStream(
    const VideoLayersAllocation& allocation) {
  SpatialLayersBitmasks result;
  for (const auto& layer : allocation.active_spatial_layers) {
    if (layer.rtp_stream_index >= kMaxNumRtpStreams ||
        layer.spatial_id >= VideoLayersAllocation::kMaxSpatialIds) {
      result.max_rtp_stream_id = -1;
      return result;
    }
    result.spatial_layer_bitmask[layer.rtp_stream_index] |=
        (1u << layer.spatial_id);
    if (result.max_rtp_stream_id < layer.rtp_stream_index) {
      result.max_rtp_stream_id = layer.rtp_stream_index;
    }
  }
  for (int i = 1; i <= result.max_rtp_stream_id; ++i) {
    if (result.spatial_layer_bitmask[i] != result.spatial_layer_bitmask[0]) {
      result.are_the_same = false;
      break;
    }
  }
  return result;
}

// TODO(bugs.webrtc.org/12000): share Leb128 functions with av1 packetizer.
int Leb128Size(uint32_t value) {
  int size = 0;
  while (value >= 0x80) {
    ++size;
    value >>= 7;
  }
  return size + 1;
}

// Returns number of bytes consumed.
int WriteLeb128(uint32_t value, uint8_t* buffer) {
  int size = 0;
  while (value >= 0x80) {
    buffer[size] = 0x80 | (value & 0x7F);
    ++size;
    value >>= 7;
  }
  buffer[size] = value;
  ++size;
  return size;
}

struct VariableLengthValue {
  size_t size = 0;
  uint64_t value = 0;
};

// Returns number of bytes consumed or 0 on error.
VariableLengthValue ReadLeb128(rtc::ArrayView<const uint8_t> buffer) {
  VariableLengthValue value;
  while (value.size < buffer.size() && value.size < 8) {
    uint8_t leb128_byte = buffer[value.size];
    value.value |= uint64_t{leb128_byte & 0x7Fu} << (value.size * 7);
    ++value.size;
    if ((leb128_byte & 0x80) == 0) {
      return value;
    }
  }
  value.size = 0;
  return value;
}

}  // namespace

// TODO(bugs.webrtc.org/12000): Review and revise the content and encoding of
// this extension. This is an experimental second version.
//                            0 1 2 3 4 5 6 7
//                           +-+-+-+-+-+-+-+-+
//                           |RID| NS| sl_bm |
//                           +-+-+-+-+-+-+-+-+
// Spatial layer bitmask     |sl0_bm |sl1_bm |
//   up to 2 bytes           |---------------|
//   when sl_bm == 0         |sl2_bm |sl3_bm |
//                           +-+-+-+-+-+-+-+-+
//   Number of temporal      |#tl|#tl|#tl|#tl|
// layers per spatial layer  :---------------:
//    up to 4 bytes          |      ...      |
//                           +-+-+-+-+-+-+-+-+
//  Target bitrate in kpbs   |               |
//   per temporal layer      :      ...      :
//    leb128 encoded         |               |
//                           +-+-+-+-+-+-+-+-+
// Resolution and framerate  |               |
// 5 bytes per spatial layer +   width for   +
//      (optional)           | rid=0, sid=0  |
//                           +---------------+
//                           |               |
//                           +  height for   +
//                           | rid=0, sid=0  |
//                           +---------------+
//                           | max framerate |
//                           +-+-+-+-+-+-+-+-+
//                           :      ...      :
//                           +-+-+-+-+-+-+-+-+
//
// RID: RTP stream index this allocation is sent on, numbered from 0. 2 bits.
// NS: Number of RTP streams - 1. 2 bits, thus allowing up-to 4 RTP streams.
// sl_bm: bitmask of active spatial layers when same for all RTP streams or
//     0 otherwise. 4 bits thus allows up to 4 spatial layers per RTP streams.
// slX_bm: bitmask of active spatial layers for RTP stream with index=X.
//     byte-aligned. When NS<=2, takes ones byte, otherwise uses two bytes.
// #tl: 2-bit value of number of temporal layers-1, thus allowing up-to 4
//     temporal layer per spatial layer. One per spatial layer per RTP stream.
//     values are stored in (RTP stream id, spatial id) ascending order.
//     zero-padded to byte alignment.
// Target bitrate in kbps. Values are stored using leb128 encoding.
//     one value per temporal layer.  values are stored in
//     (RTP stream id, spatial id, temporal id) ascending order.
//     All bitrates are total required bitrate to receive the corresponding
//     layer, i.e. in simulcast mode they include only corresponding spatial
//     layer, in full-svc all lower spatial layers are included. All lower
//     temporal layers are also included.
// Resolution and framerate.
//     Optional. Presense is infered from the rtp header extension size.
//     Encoded width, 16-bit, height, 16-bit,  max frame rate 8-bit
//     per spatial layer per RTP stream.
//     Values are stored in (RTP stream id, spatial id) ascending order.

bool RtpVideoLayersAllocationExtension::Write(
    rtc::ArrayView<uint8_t> data,
    const VideoLayersAllocation& allocation) {
  RTC_DCHECK_LT(allocation.rtp_stream_index,
                VideoLayersAllocation::kMaxSpatialIds);
  RTC_CHECK_GE(data.size(), ValueSize(allocation));

  if (allocation.active_spatial_layers.empty()) {
    return false;
  }
  // Since all multivalue fields are stored in (rtp_stream_id, spatial_id) order
  // assume `allocation.active_spatial_layers` is already sorted. It is simpler
  // to assemble it in the sorted way than to resort during serialization.
  auto cmp = [](const VideoLayersAllocation::SpatialLayer& lhs,
                const VideoLayersAllocation::SpatialLayer& rhs) {
    return std::make_tuple(lhs.rtp_stream_index, lhs.spatial_id) <
           std::make_tuple(rhs.rtp_stream_index, rhs.spatial_id);
  };
  RTC_DCHECK(absl::c_is_sorted(allocation.active_spatial_layers, cmp));

  SpatialLayersBitmasks slb = SpatialLayersBitmasksPerRtpStream(allocation);
  if (slb.max_rtp_stream_id < 0) {
    // Invalid.
    return false;
  }

  data[0] = (allocation.rtp_stream_index << 6);
  data[0] |= slb.max_rtp_stream_id << 4;
  size_t offset = 1;
  if (slb.are_the_same) {
    data[0] |= slb.spatial_layer_bitmask[0];
  } else {
    data[1] =
        (slb.spatial_layer_bitmask[0] << 4) | slb.spatial_layer_bitmask[1];
    if (slb.max_rtp_stream_id >= 2) {
      data[2] =
          (slb.spatial_layer_bitmask[2] << 4) | slb.spatial_layer_bitmask[3];
      offset = 3;
    } else {
      offset = 2;
    }
  }

  {  // Save number of temporal layers
    int bit_offset = 8;
    data[offset] = 0;
    for (const auto& layer : allocation.active_spatial_layers) {
      if (bit_offset == 0) {
        bit_offset = 6;
        data[++offset] = 0;
      } else {
        bit_offset -= 2;
      }
      data[offset] |=
          ((layer.target_bitrate_per_temporal_layer.size() - 1) << bit_offset);
    }
    ++offset;
  }

  // Target bitrate
  for (const auto& spatial_layer : allocation.active_spatial_layers) {
    for (const DataRate& bitrate :
         spatial_layer.target_bitrate_per_temporal_layer) {
      offset += WriteLeb128(bitrate.kbps(), data.data() + offset);
    }
  }

  if (allocation.resolution_and_frame_rate_is_valid) {
    for (const auto& spatial_layer : allocation.active_spatial_layers) {
      ByteWriter<uint16_t>::WriteBigEndian(data.data() + offset,
                                           spatial_layer.width);
      offset += 2;
      ByteWriter<uint16_t>::WriteBigEndian(data.data() + offset,
                                           spatial_layer.height);
      offset += 2;
      data[offset] = spatial_layer.frame_rate_fps;
      offset += 1;
    }
  }
  RTC_DCHECK_EQ(offset, ValueSize(allocation));
  return true;
}

bool RtpVideoLayersAllocationExtension::Parse(
    rtc::ArrayView<const uint8_t> data,
    VideoLayersAllocation* allocation) {
  if (data.empty() || allocation == nullptr) {
    return false;
  }
  allocation->active_spatial_layers.clear();
  allocation->rtp_stream_index = data[0] >> 6;
  int num_rtp_streams = 1 + ((data[0] >> 4) & 0b11);
  uint8_t spatial_layers_bitmasks[kMaxNumRtpStreams];
  spatial_layers_bitmasks[0] = data[0] & 0b1111;
  size_t offset = 1;

  if (spatial_layers_bitmasks[0] != 0) {
    for (int i = 1; i < num_rtp_streams; ++i) {
      spatial_layers_bitmasks[i] = spatial_layers_bitmasks[0];
    }
  } else {
    if (data.size() <= offset) {
      return false;
    }
    spatial_layers_bitmasks[0] = data[offset] >> 4;
    spatial_layers_bitmasks[1] = data[offset] & 0b1111;
    ++offset;
    if (num_rtp_streams > 2) {
      if (data.size() <= offset) {
        return false;
      }
      spatial_layers_bitmasks[2] = data[offset] >> 4;
      spatial_layers_bitmasks[3] = data[offset] & 0b1111;
      ++offset;
    }
  }

  int bit_offset = 8;
  for (int stream_idx = 0; stream_idx < num_rtp_streams; ++stream_idx) {
    for (int sid = 0; sid < VideoLayersAllocation::kMaxSpatialIds; ++sid) {
      if ((spatial_layers_bitmasks[stream_idx] & (1 << sid)) == 0) {
        continue;
      }

      if (bit_offset == 0) {
        bit_offset = 6;
        if (++offset == data.size()) {
          return false;
        }
      } else {
        bit_offset -= 2;
      }
      int num_temporal_layers = 1 + ((data[offset] >> bit_offset) & 0b11);
      allocation->active_spatial_layers.emplace_back();
      auto& layer = allocation->active_spatial_layers.back();
      layer.rtp_stream_index = stream_idx;
      layer.spatial_id = sid;
      layer.target_bitrate_per_temporal_layer.resize(num_temporal_layers,
                                                     DataRate::Zero());
    }
  }
  if (++offset == data.size()) {
    return false;
  }

  for (auto& layer : allocation->active_spatial_layers) {
    for (DataRate& rate : layer.target_bitrate_per_temporal_layer) {
      VariableLengthValue value = ReadLeb128(data.subview(offset));
      if (value.size == 0) {
        return false;
      }
      rate = DataRate::KilobitsPerSec(value.value);
      offset += value.size;
    }
  }

  if (offset == data.size()) {
    // allocation->resolution_and_frame_rate_is_valid = false;
    return true;
  }

  if (offset + 5 * allocation->active_spatial_layers.size() != data.size()) {
    // data is left, but it size is not what can be used for resolutions and
    // framerates.
    return false;
  }
  allocation->resolution_and_frame_rate_is_valid = true;
  for (auto& layer : allocation->active_spatial_layers) {
    layer.width = ByteReader<uint16_t, 2>::ReadBigEndian(data.data() + offset);
    offset += 2;
    layer.height = ByteReader<uint16_t, 2>::ReadBigEndian(data.data() + offset);
    offset += 2;
    layer.frame_rate_fps = data[offset];
    ++offset;
  }
  return true;
}

size_t RtpVideoLayersAllocationExtension::ValueSize(
    const VideoLayersAllocation& allocation) {
  if (allocation.active_spatial_layers.empty()) {
    return 0;
  }
  size_t result = 1;  // header
  SpatialLayersBitmasks slb = SpatialLayersBitmasksPerRtpStream(allocation);
  if (slb.max_rtp_stream_id < 0) {
    // Invalid.
    return 0;
  }
  if (!slb.are_the_same) {
    ++result;
    if (slb.max_rtp_stream_id >= 2) {
      ++result;
    }
  }
  // 2 bits per active spatial layer, rounded up to full byte, i.e.
  // 0.25 byte per active spatial layer.
  result += (allocation.active_spatial_layers.size() + 3) / 4;
  for (const auto& spatial_layer : allocation.active_spatial_layers) {
    for (DataRate value : spatial_layer.target_bitrate_per_temporal_layer) {
      result += Leb128Size(value.kbps());
    }
  }
  if (allocation.resolution_and_frame_rate_is_valid) {
    result += 5 * allocation.active_spatial_layers.size();
  }
  return result;
}

}  // namespace webrtc
