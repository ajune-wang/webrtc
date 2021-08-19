/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

#include "absl/strings/string_view.h"
#include "rtc_base/logging.h"
#include "rtc_base/memory/bit_reader.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {
constexpr size_t kVp9NumRefsPerFrame = 3;
constexpr size_t kVp9MaxRefLFDeltas = 4;
constexpr size_t kVp9MaxModeLFDeltas = 2;
constexpr size_t kVp9MinTileWidthB64 = 4;
constexpr size_t kVp9MaxTileWidthB64 = 64;

void Vp9ReadColorConfig(BitReader& br, Vp9UncompressedHeader& frame_info) {
  if (frame_info.profile == 2 || frame_info.profile == 3) {
    frame_info.bit_detph =
        br.ReadBits(1) ? Vp9BitDept::k12Bit : Vp9BitDept::k10Bit;
  } else {
    frame_info.bit_detph = Vp9BitDept::k8Bit;
  }

  frame_info.color_space = static_cast<Vp9ColorSpace>(br.ReadBits(3));

  if (frame_info.color_space != Vp9ColorSpace::CS_RGB) {
    frame_info.color_range =
        br.ReadBits(1) ? Vp9ColorRange::kFull : Vp9ColorRange::kStudio;

    if (frame_info.profile == 1 || frame_info.profile == 3) {
      static constexpr Vp9YuvSubsampling kSubSamlings[] = {
          Vp9YuvSubsampling::k444, Vp9YuvSubsampling::k440,
          Vp9YuvSubsampling::k422, Vp9YuvSubsampling::k420};
      frame_info.sub_sampling = kSubSamlings[br.ReadBits(2)];

      if (br.ReadBits(1) != 0) {
        RTC_LOG(LS_WARNING) << "Failed to parse header. Reserved bit set.";
        br.Invalidate();
        return;
      }
    } else {
      // Profile 0 or 2.
      frame_info.sub_sampling = Vp9YuvSubsampling::k420;
    }
  } else {
    // SRGB
    frame_info.color_range = Vp9ColorRange::kFull;
    if (frame_info.profile == 1 || frame_info.profile == 3) {
      frame_info.sub_sampling = Vp9YuvSubsampling::k444;
      if (br.ReadBits(1) != 0) {
        RTC_LOG(LS_WARNING) << "Failed to parse header. Reserved bit set.";
        br.Invalidate();
        return;
      }
    } else {
      RTC_LOG(LS_WARNING) << "Failed to parse header. 4:4:4 color not supported"
                             " in profile 0 or 2.";
      br.Invalidate();
      return;
    }
  }
}

void ReadRefreshFrameFlags(BitReader& br, Vp9UncompressedHeader& frame_info) {
  // Refresh frame flags.
  uint8_t flags = br.ReadUInt8();
  for (int i = 0; i < 8; ++i) {
    frame_info.updated_buffers.set(i, (flags & (0x01 << (7 - i))) != 0);
  }
}

void Vp9ReadFrameSize(BitReader& br, Vp9UncompressedHeader& frame_info) {
  // 16 bits: frame (width|height) - 1.
  frame_info.frame_width = br.ReadUInt16() + 1;
  frame_info.frame_height = br.ReadUInt16() + 1;
}

void Vp9ReadRenderSize(BitReader& br, Vp9UncompressedHeader& frame_info) {
  if (br.ReadBool()) {
    // render_and_frame_size_different
    // TODO(danilchap): ???
    //    auto& pos = frame_info.render_size_position.emplace();
    //    br->GetPosition(&pos.byte_offset, &pos.bit_offset);
    // 16 bits: render (width|height) - 1.
    frame_info.render_width = br.ReadUInt16() + 1;
    frame_info.render_height = br.ReadUInt16() + 1;
  } else {
    frame_info.render_width = frame_info.frame_width;
    frame_info.render_height = frame_info.frame_height;
  }
}

void Vp9ReadFrameSizeFromRefs(BitReader& br,
                              Vp9UncompressedHeader& frame_info) {
  bool found_ref = false;
  for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
    // Size in refs.
    if (br.ReadBool()) {
      frame_info.infer_size_from_reference = frame_info.reference_buffers[i];
      found_ref = true;
      break;
    }
  }

  if (!found_ref) {
    Vp9ReadFrameSize(br, frame_info);
  }
  Vp9ReadRenderSize(br, frame_info);
}

void Vp9ReadLoopfilter(BitReader& br) {
  // 6 bits: filter level.
  // 3 bits: sharpness level.
  br.Consume(9);

  if (br.ReadBool()) {    // if mode_ref_delta_enabled
    if (br.ReadBool()) {  // if mode_ref_delta_update
      for (size_t i = 0; i < kVp9MaxRefLFDeltas; i++) {
        if (br.ReadBool()) {
          br.ConsumeBits(7);
        }
      }
      for (size_t i = 0; i < kVp9MaxModeLFDeltas; i++) {
        if (br.ReadBool()) {
          br.ConsumeBits(7);
        }
      }
    }
  }
}

void Vp9ReadQp(BitReader& br, Vp9UncompressedHeader& frame_info) {
  frame_info.base_qp = br.ReadUInt8();

  // yuv offsets
  frame_info.is_lossless = frame_info.base_qp == 0;
  for (int i = 0; i < 3; ++i) {
    if (br.ReadBool()) {  // if delta_coded
      if (br.ReadBits(4) != 0) {
        frame_info.is_lossless = false;
      }
    }
  }
}

void Vp9ReadSegmentationParams(BitReader& br,
                               Vp9UncompressedHeader& frame_info) {
  constexpr int kSegmentationFeatureBits[kVp9SegLvlMax] = {8, 6, 2, 0};
  constexpr bool kSegmentationFeatureSigned[kVp9SegLvlMax] = {1, 1, 0, 0};

  if (!br.ReadBool()) {
    return;
  }
  // segmentation_enabled
  frame_info.segmentation_enabled = true;
  if (br.ReadBool()) {  // update_map
    frame_info.segmentation_tree_probs.emplace();
    for (int i = 0; i < 7; ++i) {
      if (br.ReadBool()) {
        (*frame_info.segmentation_tree_probs)[i] = br.ReadUInt8();
      } else {
        (*frame_info.segmentation_tree_probs)[i] = 255;
      }
    }

    // temporal_update
    frame_info.segmentation_pred_prob.emplace();
    if (br.ReadBool()) {
      for (int i = 0; i < 3; ++i) {
        if (br.ReadBool()) {
          (*frame_info.segmentation_pred_prob)[i] = br.ReadUInt8();
        } else {
          (*frame_info.segmentation_pred_prob)[i] = 255;
        }
      }
    } else {
      frame_info.segmentation_pred_prob->fill(255);
    }
  }

  if (br.ReadBool()) {  // segmentation_update_data
    frame_info.segmentation_is_delta = br.ReadBool();

    for (size_t i = 0; i < kVp9MaxSegments; ++i) {
      for (size_t j = 0; j < kVp9SegLvlMax; ++j) {
        if (br.ReadBool()) {  // feature_enabled
          if (kSegmentationFeatureBits[j] == 0) {
            // No feature bits used and no sign, just mark it and return.
            frame_info.segmentation_features[i][j] = 1;
          }
          frame_info.segmentation_features[i][j] =
              br.ReadBits(kSegmentationFeatureBits[j]);
          if (kSegmentationFeatureSigned[j]) {
            if (br.ReadBool()) {
              (*frame_info.segmentation_features[i][j]) *= -1;
            }
          }
        }
      }
    }
  }
}

void Vp9ReadTileInfo(BitReader& br, Vp9UncompressedHeader& frame_info) {
  size_t mi_cols = (frame_info.frame_width + 7) >> 3;
  size_t sb64_cols = (mi_cols + 7) >> 3;

  size_t min_log2 = 0;
  while ((kVp9MaxTileWidthB64 << min_log2) < sb64_cols) {
    ++min_log2;
  }

  size_t max_log2 = 1;
  while ((sb64_cols >> max_log2) >= kVp9MinTileWidthB64) {
    ++max_log2;
  }
  --max_log2;

  frame_info.tile_cols_log2 = min_log2;
  while (frame_info.tile_cols_log2 < max_log2) {
    if (br.ReadBool()) {
      ++frame_info.tile_cols_log2;
    } else {
      break;
    }
  }
  frame_info.tile_rows_log2 = 0;
  if (br.ReadBool()) {
    ++frame_info.tile_rows_log2;
    if (br.ReadBool()) {
      ++frame_info.tile_rows_log2;
    }
  }
}

const Vp9InterpolationFilter kLiteralToType[4] = {
    Vp9InterpolationFilter::kEightTapSmooth, Vp9InterpolationFilter::kEightTap,
    Vp9InterpolationFilter::kEightTapSharp, Vp9InterpolationFilter::kBilinear};
}  // namespace

std::string Vp9UncompressedHeader::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder oss(buf);

  oss << "Vp9UncompressedHeader { "
      << "profile = " << profile;

  if (show_existing_frame) {
    oss << ", show_existing_frame = " << *show_existing_frame << " }";
    return oss.str();
  }

  oss << ", frame type = " << (is_keyframe ? "key" : "delta")
      << ", show_frame = " << (show_frame ? "true" : "false")
      << ", error_resilient = " << (error_resilient ? "true" : "false");

  oss << ", bit_depth = ";
  switch (bit_detph) {
    case Vp9BitDept::k8Bit:
      oss << "8bit";
      break;
    case Vp9BitDept::k10Bit:
      oss << "10bit";
      break;
    case Vp9BitDept::k12Bit:
      oss << "12bit";
      break;
  }

  if (color_space) {
    oss << ", color_space = ";
    switch (*color_space) {
      case Vp9ColorSpace::CS_UNKNOWN:
        oss << "unknown";
        break;
      case Vp9ColorSpace::CS_BT_601:
        oss << "CS_BT_601 Rec. ITU-R BT.601-7";
        break;
      case Vp9ColorSpace::CS_BT_709:
        oss << "Rec. ITU-R BT.709-6";
        break;
      case Vp9ColorSpace::CS_SMPTE_170:
        oss << "SMPTE-170";
        break;
      case Vp9ColorSpace::CS_SMPTE_240:
        oss << "SMPTE-240";
        break;
      case Vp9ColorSpace::CS_BT_2020:
        oss << "Rec. ITU-R BT.2020-2";
        break;
      case Vp9ColorSpace::CS_RESERVED:
        oss << "Reserved";
        break;
      case Vp9ColorSpace::CS_RGB:
        oss << "sRGB (IEC 61966-2-1)";
        break;
    }
  }

  if (color_range) {
    oss << ", color_range = ";
    switch (*color_range) {
      case Vp9ColorRange::kFull:
        oss << "full";
        break;
      case Vp9ColorRange::kStudio:
        oss << "studio";
        break;
    }
  }

  if (sub_sampling) {
    oss << ", sub_sampling = ";
    switch (*sub_sampling) {
      case Vp9YuvSubsampling::k444:
        oss << "444";
        break;
      case Vp9YuvSubsampling::k440:
        oss << "440";
        break;
      case Vp9YuvSubsampling::k422:
        oss << "422";
        break;
      case Vp9YuvSubsampling::k420:
        oss << "420";
        break;
    }
  }

  if (infer_size_from_reference) {
    oss << ", infer_frame_resolution_from = " << *infer_size_from_reference;
  } else {
    oss << ", frame_width = " << frame_width
        << ", frame_height = " << frame_height;
  }
  if (render_width != 0 && render_height != 0) {
    oss << ", render_width = " << render_width
        << ", render_height = " << render_height;
  }

  oss << ", base qp = " << base_qp;
  if (reference_buffers[0] != -1) {
    oss << ", last_buffer = " << reference_buffers[0];
  }
  if (reference_buffers[1] != -1) {
    oss << ", golden_buffer = " << reference_buffers[1];
  }
  if (reference_buffers[2] != -1) {
    oss << ", altref_buffer = " << reference_buffers[2];
  }

  oss << ", updated buffers = { ";
  bool first = true;
  for (int i = 0; i < 8; ++i) {
    if (updated_buffers.test(i)) {
      if (first) {
        first = false;
      } else {
        oss << ", ";
      }
      oss << i;
    }
  }
  oss << " }";

  oss << ", compressed_header_size_bytes = " << compressed_header_size;

  oss << " }";
  return oss.str();
}

bool Parse(rtc::ArrayView<const uint8_t> buf,
           Vp9UncompressedHeader& frame_info,
           bool qp_only) {
  BitReader br(buf);

  // Frame marker.
  if (br.ReadBits(2) != 0x2) {
    RTC_LOG(LS_WARNING) << "Failed to parse header. Frame marker should be 2.";
    return false;
  }

  // Profile has low bit first.
  frame_info.profile = br.ReadBits(1);
  frame_info.profile |= (br.ReadBits(1) << 1);
  if (frame_info.profile > 2 && br.ReadBits(1) != 0) {
    RTC_LOG(LS_WARNING) << "Failed to get QP. Unsupported bitstream profile.";
    return false;
  }

  // Show existing frame.
  if (br.ReadBool()) {
    frame_info.show_existing_frame = br.ReadBits(3);
    return br.Ok();
  }

  frame_info.is_keyframe = br.ReadBits(1) == 0;
  frame_info.show_frame = br.ReadBool();
  frame_info.error_resilient = br.ReadBool();

  if (frame_info.is_keyframe) {
    if (br.ReadBits(24) != 0x498342) {
      RTC_LOG(LS_WARNING) << "Failed to get QP. Invalid sync code.";
      return false;
    }

    Vp9ReadColorConfig(br, frame_info);
    Vp9ReadFrameSize(br, frame_info);
    Vp9ReadRenderSize(br, frame_info);

    // Key-frames implicitly update all buffers.
    frame_info.updated_buffers.set();
  } else {
    // Non-keyframe.
    bool is_intra_only = false;
    if (!frame_info.show_frame) {
      is_intra_only = br.ReadBool();
    }
    if (!frame_info.error_resilient) {
      br.ConsumeBits(2);  // Reset frame context.
    }

    if (is_intra_only) {
      if (br.ReadBits(24) != 0x498342) {
        RTC_LOG(LS_WARNING) << "Failed to get QP. Invalid sync code.";
        return false;
      }

      if (frame_info.profile > 0) {
        Vp9ReadColorConfig(br, frame_info);
      } else {
        frame_info.color_space = Vp9ColorSpace::CS_BT_601;
        frame_info.sub_sampling = Vp9YuvSubsampling::k420;
        frame_info.bit_detph = Vp9BitDept::k8Bit;
      }
      frame_info.reference_buffers.fill(-1);
      ReadRefreshFrameFlags(br, frame_info);
      Vp9ReadFrameSize(br, frame_info);
      Vp9ReadRenderSize(br, frame_info);
    } else {
      ReadRefreshFrameFlags(br, frame_info);

      frame_info.reference_buffers_sign_bias[0] = false;
      for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
        frame_info.reference_buffers[i] = br.ReadBits(3);
        frame_info.reference_buffers_sign_bias[Vp9ReferenceFrame::kLast + i] =
            br.ReadBool();
      }

      Vp9ReadFrameSizeFromRefs(br, frame_info);
      frame_info.allow_high_precision_mv = br.ReadBool();

      // Interpolation filter.
      if (br.ReadBool()) {
        frame_info.interpolation_filter = Vp9InterpolationFilter::kSwitchable;
      } else {
        frame_info.interpolation_filter = kLiteralToType[br.ReadBits(2)];
      }
    }
  }

  if (!frame_info.error_resilient) {
    // 1 bit: Refresh frame context.
    // 1 bit: Frame parallel decoding mode.
    br.Consume(2);
  }

  // Frame context index.
  frame_info.frame_context_idx = br.ReadBits(2);

  Vp9ReadLoopfilter(br);

  // Read base QP.
  Vp9ReadQp(br, frame_info);

  if (qp_only) {
    // Not interested in the rest of the header, return early.
    return br.Ok();
  }

  Vp9ReadSegmentationParams(br, frame_info);
  Vp9ReadTileInfo(br, frame_info);
  frame_info.compressed_header_size = br.ReadUInt16();

  if (!br.Ok()) {
    return false;
  }
  // Trailing bits.
  br.Consume(br.RemainingBitCount() % 8);
  frame_info.uncompressed_header_size =
      buf.size() - (br.RemainingBitCount() / 8);

  RTC_DCHECK(br.Ok());
  return true;
}

absl::optional<Vp9UncompressedHeader> ParseUncompressedVp9Header(
    rtc::ArrayView<const uint8_t> buf) {
  Vp9UncompressedHeader frame_info;
  if (Parse(buf, frame_info, /*qp_only=*/false) && frame_info.frame_width > 0) {
    return frame_info;
  }
  return absl::nullopt;
}

namespace vp9 {

bool GetQp(const uint8_t* buf, size_t length, int* qp) {
  Vp9UncompressedHeader frame_info;
  if (!Parse(rtc::MakeArrayView(buf, length), frame_info, /*qp_only=*/true)) {
    return false;
  }
  *qp = frame_info.base_qp;
  return true;
}

}  // namespace vp9
}  // namespace webrtc
