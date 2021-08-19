/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_reader.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

RtpDependencyDescriptorReader::RtpDependencyDescriptorReader(
    rtc::ArrayView<const uint8_t> raw_data,
    const FrameDependencyStructure* structure,
    DependencyDescriptor* descriptor)
    : descriptor_(descriptor), buffer_(raw_data) {
  RTC_DCHECK(descriptor);

  ReadMandatoryFields();
  if (raw_data.size() > 3)
    ReadExtendedFields();

  structure_ = descriptor->attached_structure
                   ? descriptor->attached_structure.get()
                   : structure;
  if (structure_ == nullptr) {
    FailParsing();
  }
  if (!ParseSuccessful()) {
    return;
  }
  if (active_decode_targets_present_flag_) {
    descriptor->active_decode_targets_bitmask =
        buffer_.Read(structure_->num_decode_targets);
  }

  ReadFrameDependencyDefinition();
}

void RtpDependencyDescriptorReader::ReadTemplateDependencyStructure() {
  descriptor_->attached_structure =
      std::make_unique<FrameDependencyStructure>();
  descriptor_->attached_structure->structure_id = buffer_.Read(6);
  descriptor_->attached_structure->num_decode_targets = buffer_.Read(5) + 1;

  ReadTemplateLayers();
  ReadTemplateDtis();
  ReadTemplateFdiffs();
  ReadTemplateChains();

  uint32_t has_resolutions = buffer_.Read(1);
  if (has_resolutions)
    ReadResolutions();
}

void RtpDependencyDescriptorReader::ReadTemplateLayers() {
  enum NextLayerIdc : uint32_t {
    kSameLayer = 0,
    kNextTemporalLayer = 1,
    kNextSpatialLayer = 2,
    kNoMoreTemplates = 3,
  };
  std::vector<FrameDependencyTemplate> templates;

  int temporal_id = 0;
  int spatial_id = 0;
  NextLayerIdc next_layer_idc;
  do {
    if (templates.size() == DependencyDescriptor::kMaxTemplates) {
      FailParsing();
      break;
    }
    templates.emplace_back();
    FrameDependencyTemplate& last_template = templates.back();
    last_template.temporal_id = temporal_id;
    last_template.spatial_id = spatial_id;

    next_layer_idc = static_cast<NextLayerIdc>(buffer_.Read(2));
    if (next_layer_idc == kNextTemporalLayer) {
      temporal_id++;
      if (temporal_id >= DependencyDescriptor::kMaxTemporalIds) {
        FailParsing();
        break;
      }
    } else if (next_layer_idc == kNextSpatialLayer) {
      temporal_id = 0;
      spatial_id++;
      if (spatial_id >= DependencyDescriptor::kMaxSpatialIds) {
        FailParsing();
        break;
      }
    }
  } while (next_layer_idc != kNoMoreTemplates && ParseSuccessful());

  descriptor_->attached_structure->templates = std::move(templates);
}

void RtpDependencyDescriptorReader::ReadTemplateDtis() {
  FrameDependencyStructure* structure = descriptor_->attached_structure.get();
  for (FrameDependencyTemplate& current_template : structure->templates) {
    current_template.decode_target_indications.resize(
        structure->num_decode_targets);
    for (int i = 0; i < structure->num_decode_targets; ++i) {
      current_template.decode_target_indications[i] =
          static_cast<DecodeTargetIndication>(buffer_.Read(2));
    }
  }
}

void RtpDependencyDescriptorReader::ReadTemplateFdiffs() {
  for (FrameDependencyTemplate& current_template :
       descriptor_->attached_structure->templates) {
    for (uint32_t fdiff_follows = buffer_.Read(1); fdiff_follows;
         fdiff_follows = buffer_.Read(1)) {
      uint32_t fdiff_minus_one = buffer_.Read(4);
      current_template.frame_diffs.push_back(fdiff_minus_one + 1);
    }
  }
}

void RtpDependencyDescriptorReader::ReadTemplateChains() {
  FrameDependencyStructure* structure = descriptor_->attached_structure.get();
  structure->num_chains =
      buffer_.ReadNonSymmetric(structure->num_decode_targets + 1);
  if (structure->num_chains == 0)
    return;
  for (int i = 0; i < structure->num_decode_targets; ++i) {
    uint32_t protected_by_chain =
        buffer_.ReadNonSymmetric(structure->num_chains);
    structure->decode_target_protected_by_chain.push_back(protected_by_chain);
  }
  for (FrameDependencyTemplate& frame_template : structure->templates) {
    for (int chain_id = 0; chain_id < structure->num_chains; ++chain_id) {
      frame_template.chain_diffs.push_back(buffer_.Read(4));
    }
  }
}

void RtpDependencyDescriptorReader::ReadResolutions() {
  FrameDependencyStructure* structure = descriptor_->attached_structure.get();
  // The way templates are bitpacked, they are always ordered by spatial_id.
  int spatial_layers = structure->templates.back().spatial_id + 1;
  structure->resolutions.reserve(spatial_layers);
  for (int sid = 0; sid < spatial_layers; ++sid) {
    uint16_t width_minus_1 = buffer_.ReadUInt16();
    uint16_t height_minus_1 = buffer_.ReadUInt16();
    structure->resolutions.emplace_back(width_minus_1 + 1, height_minus_1 + 1);
  }
}

void RtpDependencyDescriptorReader::ReadMandatoryFields() {
  descriptor_->first_packet_in_frame = buffer_.Read(1);
  descriptor_->last_packet_in_frame = buffer_.Read(1);
  frame_dependency_template_id_ = buffer_.Read(6);
  descriptor_->frame_number = buffer_.ReadUInt16();
}

void RtpDependencyDescriptorReader::ReadExtendedFields() {
  bool template_dependency_structure_present_flag = buffer_.Read(1);
  active_decode_targets_present_flag_ = buffer_.Read(1);
  custom_dtis_flag_ = buffer_.Read(1);
  custom_fdiffs_flag_ = buffer_.Read(1);
  custom_chains_flag_ = buffer_.Read(1);
  if (template_dependency_structure_present_flag) {
    ReadTemplateDependencyStructure();
    RTC_DCHECK(descriptor_->attached_structure);
    descriptor_->active_decode_targets_bitmask =
        (uint64_t{1} << descriptor_->attached_structure->num_decode_targets) -
        1;
  }
}

void RtpDependencyDescriptorReader::ReadFrameDependencyDefinition() {
  size_t template_index =
      (frame_dependency_template_id_ + DependencyDescriptor::kMaxTemplates -
       structure_->structure_id) %
      DependencyDescriptor::kMaxTemplates;

  if (template_index >= structure_->templates.size()) {
    FailParsing();
    return;
  }

  // Copy all the fields from the matching template
  descriptor_->frame_dependencies = structure_->templates[template_index];

  if (custom_dtis_flag_)
    ReadFrameDtis();
  if (custom_fdiffs_flag_)
    ReadFrameFdiffs();
  if (custom_chains_flag_)
    ReadFrameChains();

  if (structure_->resolutions.empty()) {
    descriptor_->resolution = absl::nullopt;
  } else {
    // Format guarantees that if there were resolutions in the last structure,
    // then each spatial layer got one.
    RTC_DCHECK_LE(descriptor_->frame_dependencies.spatial_id,
                  structure_->resolutions.size());
    descriptor_->resolution =
        structure_->resolutions[descriptor_->frame_dependencies.spatial_id];
  }
}

void RtpDependencyDescriptorReader::ReadFrameDtis() {
  RTC_DCHECK_EQ(
      descriptor_->frame_dependencies.decode_target_indications.size(),
      structure_->num_decode_targets);
  for (auto& dti : descriptor_->frame_dependencies.decode_target_indications) {
    dti = static_cast<DecodeTargetIndication>(buffer_.Read(2));
  }
}

void RtpDependencyDescriptorReader::ReadFrameFdiffs() {
  descriptor_->frame_dependencies.frame_diffs.clear();
  for (uint32_t next_fdiff_size = buffer_.Read(2); next_fdiff_size > 0;
       next_fdiff_size = buffer_.Read(2)) {
    uint32_t fdiff_minus_one = buffer_.Read(4 * next_fdiff_size);
    descriptor_->frame_dependencies.frame_diffs.push_back(fdiff_minus_one + 1);
  }
}

void RtpDependencyDescriptorReader::ReadFrameChains() {
  RTC_DCHECK_EQ(descriptor_->frame_dependencies.chain_diffs.size(),
                structure_->num_chains);
  for (auto& chain_diff : descriptor_->frame_dependencies.chain_diffs) {
    chain_diff = buffer_.ReadUInt8();
  }
}

}  // namespace webrtc
