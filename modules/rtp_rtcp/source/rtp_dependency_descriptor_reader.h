/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "rtc_base/bit_buffer.h"

namespace webrtc {
// Deserializes DependencyDescriptor rtp header extension.
class RtpDependencyDescriptorReader {
 public:
  // Parses the dependency descriptor.
  RtpDependencyDescriptorReader(
      rtc::ArrayView<const uint8_t> raw_data,
      const FrameDependencyStructure* latest_structure,
      DependencyDescriptor* descriptor);
  RtpDependencyDescriptorReader(const RtpDependencyDescriptorReader&) = delete;
  RtpDependencyDescriptorReader& operator=(
      const RtpDependencyDescriptorReader&) = delete;

  // Returns if parse was successful.
  bool Parsed() { return parsed_; }

 private:
  uint32_t ReadBits(size_t bit_count);
  uint32_t ReadNonSymmetric(size_t num_values);

  // Functions to read template dependency structure.
  std::unique_ptr<FrameDependencyStructure> ReadTemplateDependencyStructure();
  std::vector<FrameDependencyTemplate> ReadTemplateLayers();
  void ReadTemplateDtis(FrameDependencyStructure* structure);
  void ReadTemplateFdiffs(FrameDependencyStructure* structure);
  void ReadTemplateChains(FrameDependencyStructure* structure);
  void ReadResolutions(FrameDependencyStructure* structure);

  // Function to read details for the current frame.
  void ReadMandatoryFields();
  void ReadExtendedFields();
  void ReadFrameDependencyDefinition();

  void ReadFrameDtis(FrameDependencyTemplate* frame);
  void ReadFrameFdiffs(FrameDependencyTemplate* frame);
  void ReadFrameChains(FrameDependencyTemplate* frame);

  // Output.
  bool parsed_ = true;
  DependencyDescriptor* const descriptor_;
  // Values that are needed while reading the descriptor, but can be discarded
  // when reading is complete.
  rtc::BitBuffer buffer_;
  int frame_dependency_template_id_ = 0;
  bool custom_dtis_flag_ = false;
  bool custom_fdiffs_flag_ = false;
  bool custom_chains_flag_ = false;
  const FrameDependencyStructure* structure_ = nullptr;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
