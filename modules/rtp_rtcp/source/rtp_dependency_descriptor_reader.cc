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

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr int kMaxTemporalId = 7;
constexpr int kMaxSpatialId = 3;
constexpr int kMaxTemplates = 63;
constexpr int kMaxTemplateId = kMaxTemplates - 1;
constexpr int kExtendedFieldsIndicator = kMaxTemplates;

bool ReadTemplateLayers(rtc::BitBuffer* buffer,
                        std::vector<FrameDependencyTemplate>* templates) {
  RTC_DCHECK(templates);
  RTC_DCHECK(templates->empty());
  int temporal_id = 0;
  int spatial_id = 0;
  while (templates->size() < kMaxTemplates) {
    templates->emplace_back();
    FrameDependencyTemplate& last_template = templates->back();
    last_template.temporal_id = temporal_id;
    last_template.spatial_id = spatial_id;

    uint32_t next_layer_idc;
    if (!buffer->ReadBits(&next_layer_idc, 2))
      return false;
    switch (next_layer_idc) {
      case 0:
        break;
      case 1:
        temporal_id++;
        if (temporal_id > kMaxTemporalId)
          return false;
        break;
      case 2:
        temporal_id = 0;
        spatial_id++;
        if (spatial_id > kMaxSpatialId)
          return false;
        break;
      case 3:
        return true;
    }
  }

  return false;
}

bool ReadFrameDtis(rtc::BitBuffer* buffer,
                   int num_decode_targets,
                   FrameDependencyTemplate* frame) {
  frame->decode_target_indications.resize(num_decode_targets);
  for (int i = 0; i < num_decode_targets; ++i) {
    uint32_t dti;
    if (!buffer->ReadBits(&dti, 2))
      return false;
    frame->decode_target_indications[i] =
        static_cast<DecodeTargetIndication>(dti);
  }
  return true;
}

bool ReadTemplateDtis(rtc::BitBuffer* buffer,
                      int num_decode_targets,
                      rtc::ArrayView<FrameDependencyTemplate> templates) {
  for (FrameDependencyTemplate& current_template : templates) {
    if (!ReadFrameDtis(buffer, num_decode_targets, &current_template))
      return false;
  }
  return true;
}

bool ReadFrameFdiffs(rtc::BitBuffer* buffer, FrameDependencyTemplate* frame) {
  frame->frame_diffs.clear();
  uint32_t next_fdiff_size;
  if (!buffer->ReadBits(&next_fdiff_size, 2))
    return false;
  while (next_fdiff_size > 0) {
    uint32_t fdiff_minus_one;
    if (!buffer->ReadBits(&fdiff_minus_one, 4 * next_fdiff_size))
      return false;
    frame->frame_diffs.push_back(fdiff_minus_one + 1);
    if (!buffer->ReadBits(&next_fdiff_size, 2))
      return false;
  }
  return true;
}

bool ReadTemplateFdiffs(rtc::BitBuffer* buffer,
                        rtc::ArrayView<FrameDependencyTemplate> templates) {
  for (FrameDependencyTemplate& current_template : templates) {
    current_template.frame_diffs.clear();
    uint32_t fdiff_follows;
    if (!buffer->ReadBits(&fdiff_follows, 1))
      return false;
    while (fdiff_follows) {
      uint32_t fdiff_minus_one;
      if (!buffer->ReadBits(&fdiff_minus_one, 4))
        return false;
      current_template.frame_diffs.push_back(fdiff_minus_one + 1);
      if (!buffer->ReadBits(&fdiff_follows, 1))
        return false;
    }
  }
  return true;
}

bool ReadResolutions(rtc::BitBuffer* buffer,
                     FrameDependencyStructure* structure) {
  structure->resolutions.clear();
  uint32_t has_resolutions;
  if (!buffer->ReadBits(&has_resolutions, 1))
    return false;
  if (has_resolutions) {
    // The way templates are bitpacked, they are always ordered by spatial_id.
    int spatial_layers = structure->templates.back().spatial_id + 1;
    structure->resolutions.reserve(spatial_layers);
    for (int sid = 0; sid < spatial_layers; ++sid) {
      uint16_t width_minus_1;
      uint16_t height_minus_1;
      if (!buffer->ReadUInt16(&width_minus_1) ||
          !buffer->ReadUInt16(&height_minus_1)) {
        return false;
      }
      structure->resolutions.emplace_back(width_minus_1 + 1,
                                          height_minus_1 + 1);
    }
  }
  return true;
}

}  // namespace

struct RtpDependencyDescriptorReader::ReadingState {
  int template_id = 0;
  bool custom_dtis = false;
  bool custom_fdiffs = false;
  bool custom_chains = false;
  int structure_size_bits = 0;
  std::unique_ptr<FrameDependencyStructure> structure;
};

bool RtpDependencyDescriptorReader::Parse(
    rtc::ArrayView<const uint8_t> raw_data,
    DependencyDescriptor* descriptor) {
  RTC_DCHECK(descriptor);
  rtc::BitBuffer bit_reader(raw_data.data(), raw_data.size());
  ReadingState reading_state;
  if (!Read(&bit_reader, &reading_state, descriptor)) {
    return false;
  }
  // Update structure if there a new one.
  if (reading_state.structure) {
    frame_dependency_structure_ = std::move(reading_state.structure);
    structure_size_bits_ = reading_state.structure_size_bits;
  }
  return true;
}

bool RtpDependencyDescriptorReader::ReadMandatoryFields(
    rtc::BitBuffer* buffer,
    ReadingState* reading_state,
    DependencyDescriptor* descriptor) {
  uint8_t flags_and_template_id;
  if (!buffer->ReadUInt8(&flags_and_template_id))
    return false;
  descriptor->first_packet_in_frame = (flags_and_template_id & 0x80) != 0;
  descriptor->last_packet_in_frame = (flags_and_template_id & 0x40) != 0;
  reading_state->template_id = flags_and_template_id & 0x3f;
  uint16_t frame_number;
  if (!buffer->ReadUInt16(&frame_number))
    return false;
  descriptor->frame_number = frame_number;
  return true;
}

bool RtpDependencyDescriptorReader::ReadTemplateChains(
    rtc::BitBuffer* buffer,
    FrameDependencyStructure* structure) {
  uint32_t value = 0;
  if (!buffer->ReadNonSymmetric(&value, structure->num_decode_targets + 1))
    return false;
  structure->num_chains = value;
  if (structure->num_chains == 0) {
    return true;
  }
  RTC_DCHECK(structure->decode_target_protected_by_chain.empty());
  for (int i = 0; i < structure->num_decode_targets; ++i) {
    if (!buffer->ReadNonSymmetric(&value, structure->num_chains + 1))
      return false;
    structure->decode_target_protected_by_chain.push_back(value);
  }
  for (FrameDependencyTemplate& frame_template : structure->templates) {
    RTC_DCHECK(frame_template.chain_diffs.empty());
    for (int chain_id = 0; chain_id < structure->num_chains; ++chain_id) {
      if (!buffer->ReadBits(&value, 4))
        return false;
      frame_template.chain_diffs.push_back(value);
    }
  }
  return true;
}

bool RtpDependencyDescriptorReader::Read(
    rtc::BitBuffer* bit_reader,
    ReadingState* reading_state,
    DependencyDescriptor* descriptor) const {
  if (!ReadMandatoryFields(bit_reader, reading_state, descriptor))
    return false;
  if (reading_state->template_id == kExtendedFieldsIndicator) {
    if (!ReadExtendedFields(bit_reader, reading_state, descriptor))
      return false;
  }
  return ReadFrameDependencyDefinition(bit_reader, *reading_state, descriptor);
}

bool RtpDependencyDescriptorReader::ReadExtendedFields(
    rtc::BitBuffer* buffer,
    ReadingState* reading_state,
    DependencyDescriptor* descriptor) const {
  uint32_t value;
  // 6 + 1 + 1 + 1 + 1 bits.
  if (!buffer->ReadBits(&value, 10))
    return false;
  // frame_dependency_template_id
  reading_state->template_id = (value >> 4);
  if (reading_state->template_id == kExtendedFieldsIndicator)
    return false;

  // template_dependency_structure_present_flag
  if ((value & (1 << 3)) != 0) {
    descriptor->has_structure_attached = true;
  }
  reading_state->custom_dtis = (value & (1 << 2)) != 0;
  reading_state->custom_fdiffs = (value & (1 << 1)) != 0;
  reading_state->custom_chains = (value & (1 << 0)) != 0;

  if (descriptor->has_structure_attached) {
    return ReadTemplateDependencyStructure(buffer, reading_state);
  }

  return true;
}

bool RtpDependencyDescriptorReader::ReadTemplateDependencyStructure(
    rtc::BitBuffer* buffer,
    ReadingState* reading_state) const {
  uint32_t template_id_offset;
  if (!buffer->ReadBits(&template_id_offset, 6))
    return false;
  if (frame_dependency_structure_ &&
      frame_dependency_structure_->structure_id ==
          static_cast<int>(template_id_offset)) {
    // Same offset as previous structure. Assume it is the same structure.
    return buffer->ConsumeBits(structure_size_bits_);
  }
  reading_state->structure = absl::make_unique<FrameDependencyStructure>();
  reading_state->structure->structure_id = template_id_offset;
  // To avoid getting into inconsistent step on invalid failure, first parse
  // everything, only then update.
  uint64_t remaining_bits_at_start_of_structure = buffer->RemainingBitCount();
  uint32_t num_decode_targets_minus_1;
  if (!buffer->ReadBits(&num_decode_targets_minus_1, 5))
    return false;
  reading_state->structure->num_decode_targets = num_decode_targets_minus_1 + 1;

  if (!ReadTemplateLayers(buffer, &reading_state->structure->templates))
    return false;

  if (frame_dependency_structure_ &&
      frame_dependency_structure_->templates.size() +
              reading_state->structure->templates.size() >
          kMaxTemplates) {
    // Reject new structure if template ids it uses overlaps with the old
    // template ids.
    return false;
  }

  // At this point of reading number of templates is fixed. Readers below fills
  // templates details, but can't change their number.
  rtc::ArrayView<FrameDependencyTemplate> templates(
      reading_state->structure->templates);
  if (!ReadTemplateDtis(buffer, reading_state->structure->num_decode_targets,
                        templates)) {
    return false;
  }
  if (!ReadTemplateFdiffs(buffer, templates))
    return false;
  if (!ReadTemplateChains(buffer, reading_state->structure.get()))
    return false;
  if (!ReadResolutions(buffer, reading_state->structure.get()))
    return false;

  // Save size of the structure (excluding template_id_offset field) to quickly
  // fast forward if some next packet repeats it.
  uint64_t remaining_bits_at_end_of_structure = buffer->RemainingBitCount();
  RTC_DCHECK_GT(remaining_bits_at_start_of_structure,
                remaining_bits_at_end_of_structure);
  reading_state->structure_size_bits =
      remaining_bits_at_start_of_structure - remaining_bits_at_end_of_structure;
  return true;
}

bool RtpDependencyDescriptorReader::ReadFrameDependencyDefinition(
    rtc::BitBuffer* buffer,
    const ReadingState& reading_state,
    DependencyDescriptor* descriptor) const {
  // If current descriptor contains new structure, should use that one. if not,
  // should  use saved one.
  const FrameDependencyStructure* structure =
      reading_state.structure ? reading_state.structure.get()
                              : frame_dependency_structure_.get();
  if (structure == nullptr) {
    return false;
  }
  size_t template_index = (reading_state.template_id + (kMaxTemplateId + 1) -
                           structure->structure_id) %
                          (kMaxTemplateId + 1);

  if (template_index >= structure->templates.size()) {
    return false;
  }

  // Copy all the fields from the matching template
  descriptor->frame_dependencies = structure->templates[template_index];

  if (reading_state.custom_dtis) {
    if (!ReadFrameDtis(buffer, structure->num_decode_targets,
                       &descriptor->frame_dependencies))
      return false;
  }
  if (reading_state.custom_fdiffs) {
    if (!ReadFrameFdiffs(buffer, &descriptor->frame_dependencies))
      return false;
  }
  if (reading_state.custom_chains) {
    for (int i = 0; i < structure->num_chains; ++i) {
      uint32_t chain_diff;
      if (!buffer->ReadBits(&chain_diff, 8))
        return false;
      descriptor->frame_dependencies.chain_diffs[i] = chain_diff;
    }
  }
  if (structure->resolutions.empty()) {
    descriptor->resolution = absl::nullopt;
  } else {
    // Format guarantees that if there were resolutions in the last structure,
    // then each spatial layer got one.
    RTC_DCHECK_LE(descriptor->frame_dependencies.spatial_id,
                  structure->resolutions.size());
    descriptor->resolution =
        structure->resolutions[descriptor->frame_dependencies.spatial_id];
  }

  return true;
}

}  // namespace webrtc
