/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_
#define COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_

#include <initializer_list>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_codec_constants.h"

namespace webrtc {

// Describes how a certain encoder buffer was used when encoding a frame.
struct CodecBufferUsage {
  constexpr CodecBufferUsage(int id, bool referenced, bool updated)
      : id(id), referenced(referenced), updated(updated) {}

  int id = 0;
  bool referenced = false;
  bool updated = false;
};

enum class ChainRelation { kNone, kStarts, kContinues };

struct GenericFrameInfo {
  static absl::InlinedVector<DecodeTargetIndication, 10> DecodeTargetInfo(
      absl::string_view indication_symbols);

  class Builder;

  GenericFrameInfo();
  GenericFrameInfo(const GenericFrameInfo&);
  ~GenericFrameInfo();

  bool is_keyframe = false;
  int64_t frame_id = 0;
  int spatial_id = 0;
  int temporal_id = 0;
  uint32_t active_decode_target_bitmask = 0xffffffff;
  absl::InlinedVector<DecodeTargetIndication, 10> decode_target_indications;
  absl::InlinedVector<ChainRelation, 4> chains;
  absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers> encoder_buffers;
};

class GenericFrameInfo::Builder {
 public:
  Builder();
  ~Builder();

  GenericFrameInfo Build() const;
  Builder& T(int temporal_id);
  Builder& S(int spatial_id);
  Builder& Dtis(absl::string_view indication_symbols);

 private:
  GenericFrameInfo info_;
};

class FrameDependencyTemplateBuilder {
 public:
  FrameDependencyTemplateBuilder() = default;

  FrameDependencyTemplate Build() && { return std::move(template_); }
  FrameDependencyTemplate Build() const& { return template_; }
  FrameDependencyTemplateBuilder& T(int temporal_id) {
    template_.temporal_id = temporal_id;
    return *this;
  }
  FrameDependencyTemplateBuilder& S(int spatial_id) {
    template_.spatial_id = spatial_id;
    return *this;
  }
  FrameDependencyTemplateBuilder& Dtis(absl::string_view indication_symbols) {
    template_.decode_target_indications =
        GenericFrameInfo::DecodeTargetInfo(indication_symbols);
    return *this;
  }
  FrameDependencyTemplateBuilder& Fdiffs(
      std::initializer_list<int> frame_diffs) {
    template_.frame_diffs.assign(frame_diffs.begin(), frame_diffs.end());
    return *this;
  }
  FrameDependencyTemplateBuilder& ChainDiffs(
      std::initializer_list<int> chain_diffs) {
    template_.chain_diffs.assign(chain_diffs.begin(), chain_diffs.end());
    return *this;
  }

 private:
  FrameDependencyTemplate template_;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_
