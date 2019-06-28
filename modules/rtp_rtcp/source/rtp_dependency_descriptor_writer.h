/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_

#include <cstdint>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {
// Serialize DependencyDescriptor with respect to set FrameDependencyStructure.
class RtpDependencyDescriptorWriter {
 public:
  // Returns minimum number of bits needed to serialize descriptor with respect
  // to current FrameDependencyStructure. Returns 0 if |descriptor| can't be
  // serialized.
  size_t ValueSizeBits(const DependencyDescriptor& descriptor) const;
  size_t ValueSizeBytes(const DependencyDescriptor& descriptor) const {
    return (ValueSizeBits(descriptor) + 7) / 8;
  }

  bool Write(const DependencyDescriptor& frame_info,
             rtc::ArrayView<uint8_t> raw_data) const;

  // Sets FrameDependencyStructure to derive individual descriptors from.
  // Returns false on failure, e.g. structure is invalid or oversized.
  bool SetStructure(const FrameDependencyStructure& structure);

 private:
  using TemplateIndex = std::vector<FrameDependencyTemplate>::const_iterator;
  struct TemplateMatch {
    // Template from the frame_dependency_structure_->templates
    TemplateIndex template_index;
    bool need_custom_fdiffs;
    bool need_custom_dtis;
    bool need_custom_chains;
    // Size in bits to store frame-specific details, i.e.
    // excluding Mandatory fields and excluding template dependency structure.
    int extra_size_bits;
  };
  TemplateMatch AdditionalSizeBits(const DependencyDescriptor& descriptor,
                                   TemplateIndex frame_template) const;
  absl::optional<TemplateMatch> FindBestTemplate(
      const DependencyDescriptor& descriptor) const;

  // Size needed to serialize |frame_dependency_structure_|.
  size_t structure_size_bits_ = 0;
  absl::optional<FrameDependencyStructure> frame_dependency_structure_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_
