/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INTERFACE_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INTERFACE_H_

#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_error_codes.h"

namespace webrtc {

class RTPFragmentationHeader;  // forward declaration

// Note: if any pointers are added to this struct, it must be fitted
// with a copy-constructor. See below.
struct CodecSpecificInfoVP8 {
  bool nonReference;
  uint8_t temporalIdx;
  bool layerSync;
  int8_t keyIdx;  // Negative value to skip keyIdx.
};

struct CodecSpecificInfoVP9 {
  bool first_frame_in_picture;  // First frame, increment picture_id.
  bool inter_pic_predicted;     // This layer frame is dependent on previously
                                // coded frame(s).
  bool flexible_mode;
  bool ss_data_available;
  bool non_ref_for_inter_layer_pred;

  uint8_t temporal_idx;
  bool temporal_up_switch;
  bool inter_layer_predicted;  // Frame is dependent on directly lower spatial
                               // layer frame.
  uint8_t gof_idx;

  // SS data.
  size_t num_spatial_layers;  // Always populated.
  bool spatial_layer_resolution_present;
  uint16_t width[kMaxVp9NumberOfSpatialLayers];
  uint16_t height[kMaxVp9NumberOfSpatialLayers];
  GofInfoVP9 gof;

  // Frame reference data.
  uint8_t num_ref_pics;
  uint8_t p_diff[kMaxVp9RefPics];

  bool end_of_picture;
};

struct CodecSpecificInfoH264 {
  H264PacketizationMode packetization_mode;
};

union CodecSpecificInfoUnion {
  CodecSpecificInfoVP8 VP8;
  CodecSpecificInfoVP9 VP9;
  CodecSpecificInfoH264 H264;
};

struct FrameEncodingInfo {
  FrameEncodingInfo();
  FrameEncodingInfo(const FrameEncodingInfo&);
  ~FrameEncodingInfo();

  struct Buffer {
    enum class Usage { kReferenced, kUpdated, kReferencedAndUpdated };

    int id;
    Usage usage;
  };

  int temporal_id = 0;
  int spatial_id = 0;
  bool discardable = false;
  absl::InlinedVector<Buffer, 8> buffers;
};

struct GenericFrameInfo {
  GenericFrameInfo();
  GenericFrameInfo(const GenericFrameInfo&);
  ~GenericFrameInfo();

  enum class OperatingPointIndication {
    kNotPresent,
    kDiscardable,
    kSwitch,
    kRequired
  };

  int temporal_id = 0;
  int spatial_id = 0;
  absl::InlinedVector<int, 10> chain_diffs;
  absl::InlinedVector<int, 10> frame_diffs;
  absl::InlinedVector<OperatingPointIndication, 10> operating_points;

  std::string ToString() const;
};

struct TemplateStructure {
  TemplateStructure();
  TemplateStructure(const TemplateStructure&);
  ~TemplateStructure();

  struct Template {
    Template();
    Template(const Template&);
    ~Template();

    GenericFrameInfo generic_frame_info;
    bool repeatable = false;
  };

  int num_operating_points = 0;
  int template_offset = 0;
  std::vector<Template> templates;
};

// Note: if any pointers are added to this struct or its sub-structs, it
// must be fitted with a copy-constructor. This is because it is copied
// in the copy-constructor of VCMEncodedFrame.
struct CodecSpecificInfo {
  CodecSpecificInfo();
  ~CodecSpecificInfo();

  VideoCodecType codecType;
  CodecSpecificInfoUnion codecSpecific;
  absl::optional<GenericFrameInfo> generic_frame_info;
  absl::optional<TemplateStructure> template_structure;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INTERFACE_H_
