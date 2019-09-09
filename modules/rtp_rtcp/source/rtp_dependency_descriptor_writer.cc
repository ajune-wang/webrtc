/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_writer.h"

#include "absl/algorithm/container.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr int kMaxDecodeTargets = 32;
constexpr int kMaxTemporalId = 7;
constexpr int kMaxSpatialId = 3;
constexpr int kMaxTemplates = 63;
constexpr int kExtendedFieldsIndicator = 63;
constexpr int kMaxWidth = 1 << 16;
constexpr int kMaxHeight = 1 << 16;

enum class NextLayerIdc : uint32_t {
  kSameLayer = 0,
  kNextTemporal = 1,
  kNewSpatial = 2,
  kNoMoreLayers = 3,
  kInvalid = 4
};

NextLayerIdc GetNextLayerIdc(const FrameDependencyTemplate& previous,
                             const FrameDependencyTemplate& next) {
  if (next.spatial_id == previous.spatial_id &&
      next.temporal_id == previous.temporal_id) {
    return NextLayerIdc::kSameLayer;
  } else if (next.spatial_id == previous.spatial_id &&
             next.temporal_id == previous.temporal_id + 1) {
    return NextLayerIdc::kNextTemporal;
  } else if (next.spatial_id == previous.spatial_id + 1 &&
             next.temporal_id == 0) {
    return NextLayerIdc::kNewSpatial;
  }
  // Everything else is unsupported.
  return NextLayerIdc::kInvalid;
}

bool IsValid(const FrameDependencyStructure& structure) {
  if (structure.num_decode_targets <= 0 ||
      structure.num_decode_targets > kMaxDecodeTargets) {
    return false;
  }
  size_t num_decode_targets = structure.num_decode_targets;
  if (structure.templates.empty() ||
      structure.templates.size() > kMaxTemplates) {
    return false;
  }
  if (structure.templates[0].spatial_id != 0 ||
      structure.templates[0].temporal_id != 0) {
    return false;
  }

  size_t max_spatial_id = 0;
  for (const FrameDependencyTemplate& frame_info : structure.templates) {
    if (frame_info.temporal_id < 0 || frame_info.temporal_id > kMaxTemporalId) {
      return false;
    }
    if (frame_info.spatial_id < 0 || frame_info.spatial_id > kMaxSpatialId) {
      return false;
    }
    if (frame_info.decode_target_indications.size() != num_decode_targets) {
      return false;
    }
    if (static_cast<int>(frame_info.chain_diffs.size()) !=
        structure.num_chains) {
      return false;
    }
    // TODO(danilchap): Verify max value for each fdiff and chain_diff.
    if (frame_info.spatial_id > static_cast<int>(max_spatial_id)) {
      max_spatial_id = frame_info.spatial_id;
    }
  }
  // Validate templates are sorted correctly and have no gaps.
  for (size_t i = 1; i < structure.templates.size(); ++i) {
    if (GetNextLayerIdc(structure.templates[i - 1], structure.templates[i]) ==
        NextLayerIdc::kInvalid) {
      return false;
    }
  }
  if (!structure.resolutions.empty() &&
      structure.resolutions.size() != max_spatial_id + 1) {
    return false;
  }
  for (const RenderResolution& resolution : structure.resolutions) {
    if (resolution.Width() <= 0 || resolution.Width() > kMaxWidth) {
      return false;
    }
    if (resolution.Height() <= 0 || resolution.Height() > kMaxHeight) {
      return false;
    }
  }
  return true;
}

bool WriteTemplateLayers(
    rtc::ArrayView<const FrameDependencyTemplate> templates,
    rtc::BitBufferWriter* buffer) {
  for (size_t i = 1; i < templates.size(); ++i) {
    uint32_t next_layer_idc =
        static_cast<uint32_t>(GetNextLayerIdc(templates[i - 1], templates[i]));
    RTC_DCHECK_LE(next_layer_idc, 3);
    if (!buffer->WriteBits(next_layer_idc, 2))
      return false;
  }
  if (!buffer->WriteBits(static_cast<uint32_t>(NextLayerIdc::kNoMoreLayers), 2))
    return false;
  return true;
}

bool WriteFrameDtis(rtc::ArrayView<const DecodeTargetIndication> dtis,
                    rtc::BitBufferWriter* buffer) {
  for (DecodeTargetIndication dti : dtis) {
    if (!buffer->WriteBits(static_cast<uint32_t>(dti), 2))
      return false;
  }
  return true;
}

bool WriteTemplateDtis(rtc::ArrayView<const FrameDependencyTemplate> templates,
                       rtc::BitBufferWriter* buffer) {
  for (const FrameDependencyTemplate& current_template : templates) {
    if (!WriteFrameDtis(current_template.decode_target_indications, buffer)) {
      return false;
    }
  }
  return true;
}

bool WriteFrameFdiffs(rtc::ArrayView<const int> frame_diffs,
                      rtc::BitBufferWriter* buffer) {
  for (uint32_t fdiff : frame_diffs) {
    RTC_DCHECK_GT(fdiff, 0);
    RTC_DCHECK_LE(fdiff, 1 << 12);
    if (fdiff <= (1 << 4)) {
      uint32_t value = (1u << 4) | (fdiff - 1);
      if (!buffer->WriteBits(value, 2 + 4))
        return false;
    } else if (fdiff <= (1 << 8)) {
      uint32_t value = (2u << 8) | (fdiff - 1);
      if (!buffer->WriteBits(value, 2 + 8))
        return false;
    } else {  // fdiff <= (1 << 12)
      uint32_t value = (3u << 12) | (fdiff - 1);
      if (!buffer->WriteBits(value, 2 + 12))
        return false;
    }
  }
  // No more diffs.
  return buffer->WriteBits(0, 2);
}

bool WriteTemplateFdiffs(
    rtc::ArrayView<const FrameDependencyTemplate> templates,
    rtc::BitBufferWriter* buffer) {
  for (const FrameDependencyTemplate& current_template : templates) {
    for (uint32_t fdiff : current_template.frame_diffs) {
      RTC_DCHECK_GT(fdiff, 0);
      RTC_DCHECK_LE(fdiff, 1 << 4);
      if (!buffer->WriteBits((1u << 4) | (fdiff - 1), 5))
        return false;
    }
    // No more diffs for current template.
    if (!buffer->WriteBits(/*val=*/0, /*bit_count=*/1))
      return false;
  }
  return true;
}

bool WriterFrameChains(rtc::ArrayView<const int> chain_diffs,
                       rtc::BitBufferWriter* buffer) {
  for (int chain_diff : chain_diffs) {
    if (!buffer->WriteBits(chain_diff, 8))
      return false;
  }
  return true;
}

bool WriteResolutions(rtc::ArrayView<const RenderResolution> resolutions,
                      rtc::BitBufferWriter* buffer) {
  if (resolutions.empty()) {
    return buffer->WriteBits(/*val=*/0, /*bit_count=*/1);
  }
  // Write has_resolutions flag.
  if (!buffer->WriteBits(/*val=*/1, /*bit_count=*/1))
    return false;
  for (const RenderResolution& resolution : resolutions) {
    if (!buffer->WriteUInt16(resolution.Width() - 1))
      return false;
    if (!buffer->WriteUInt16(resolution.Height() - 1))
      return false;
  }
  return true;
}

bool WriteTemplateChains(const FrameDependencyStructure& structure,
                         rtc::BitBufferWriter* bit_writer) {
  if (!bit_writer->WriteNonSymmetric(structure.num_chains,
                                     structure.num_decode_targets + 1)) {
    return false;
  }
  if (structure.num_chains == 0) {
    return true;
  }
  RTC_DCHECK_EQ(structure.decode_target_protected_by_chain.size(),
                structure.num_decode_targets);
  for (int protected_by : structure.decode_target_protected_by_chain) {
    if (!bit_writer->WriteNonSymmetric(protected_by, structure.num_chains + 1))
      return false;
  }
  for (const auto& frame_template : structure.templates) {
    RTC_DCHECK_EQ(frame_template.chain_diffs.size(), structure.num_chains);
    for (auto chain_diff : frame_template.chain_diffs) {
      if (!bit_writer->WriteBits(chain_diff, 4))
        return false;
    }
  }
  return true;
}

bool WriteTemplateDependencyStructure(
    rtc::BitBufferWriter* bit_writer,
    uint32_t template_id_offset,
    const FrameDependencyStructure& structure) {
  if (!bit_writer->WriteBits(template_id_offset, 6))
    return false;
  if (!bit_writer->WriteBits(structure.num_decode_targets - 1, 5))
    return false;
  if (!WriteTemplateLayers(structure.templates, bit_writer))
    return false;
  if (!WriteTemplateDtis(structure.templates, bit_writer))
    return false;
  if (!WriteTemplateFdiffs(structure.templates, bit_writer))
    return false;
  if (!WriteTemplateChains(structure, bit_writer))
    return false;
  if (!WriteResolutions(structure.resolutions, bit_writer))
    return false;
  return true;
}

// Calculates numbers of bits needed to write |structure|
int StructureSizeBits(const FrameDependencyStructure& structure) {
  // template_id offset (6 bits) and number of decode targets (5 bits)
  int bits = 11;
  // template layers.
  bits += 2 * structure.templates.size();
  // dtis.
  bits += 2 * structure.templates.size() * structure.num_decode_targets;
  // fdiffs. each templates uses 1 + 5 * sizeof(fdiff) bits.
  bits += structure.templates.size();
  for (const FrameDependencyTemplate& frame_template : structure.templates) {
    bits += 5 * frame_template.frame_diffs.size();
  }
  bits += rtc::BitBufferWriter::SizeNonSymmetricBits(
      structure.num_chains, structure.num_decode_targets + 1);
  if (structure.num_chains > 0) {
    for (auto protected_by : structure.decode_target_protected_by_chain) {
      bits += rtc::BitBufferWriter::SizeNonSymmetricBits(
          protected_by, structure.num_chains + 1);
    }
    bits += 4 * structure.templates.size() * structure.num_chains;
  }
  // Resolutions.
  bits += 1 + 32 * structure.resolutions.size();
  return bits;
}

}  // namespace

RtpDependencyDescriptorWriter::TemplateMatch
RtpDependencyDescriptorWriter::AdditionalSizeBits(
    const DependencyDescriptor& descriptor,
    RtpDependencyDescriptorWriter::TemplateIndex frame_template) const {
  TemplateMatch result;
  result.template_index = frame_template;
  result.need_custom_fdiffs =
      descriptor.frame_dependencies.frame_diffs != frame_template->frame_diffs;
  result.need_custom_dtis =
      descriptor.frame_dependencies.decode_target_indications !=
      frame_template->decode_target_indications;
  result.need_custom_chains =
      descriptor.frame_dependencies.chain_diffs != frame_template->chain_diffs;

  if (!result.need_custom_fdiffs && !result.need_custom_dtis &&
      !result.need_custom_chains) {
    // perfect match.
    result.extra_size_bits = 0;
    return result;
  }
  // If structure should be attached, then there will be ExtendedFields anyway,
  // so do not count 10 bits for them as extra.
  result.extra_size_bits = descriptor.has_structure_attached ? 0 : 10;
  if (result.need_custom_fdiffs) {
    result.extra_size_bits +=
        2 * (1 + descriptor.frame_dependencies.frame_diffs.size());
    for (int fdiff : descriptor.frame_dependencies.frame_diffs) {
      if (fdiff <= (1 << 4))
        result.extra_size_bits += 4;
      else if (fdiff <= (1 << 8))
        result.extra_size_bits += 8;
      else
        result.extra_size_bits += 12;
    }
  }
  if (result.need_custom_dtis) {
    result.extra_size_bits +=
        2 * descriptor.frame_dependencies.decode_target_indications.size();
  }
  if (result.need_custom_chains) {
    result.extra_size_bits += 8 * frame_dependency_structure_->num_chains;
  }
  return result;
}

size_t RtpDependencyDescriptorWriter::ValueSizeBits(
    const DependencyDescriptor& descriptor) const {
  auto best_template = FindBestTemplate(descriptor);
  if (best_template == absl::nullopt) {
    // Can't serialize this descriptor.
    return 0;
  }
  size_t value_size_bits = 24 + best_template->extra_size_bits;
  if (descriptor.has_structure_attached) {
    value_size_bits += 10 + structure_size_bits_;
  }
  // round up to byte alignment.
  return value_size_bits;
}

bool RtpDependencyDescriptorWriter::Write(
    const DependencyDescriptor& descriptor,
    rtc::ArrayView<uint8_t> raw_data) const {
  auto best_template = FindBestTemplate(descriptor);
  if (best_template == absl::nullopt) {
    return false;
  }
  uint8_t template_id = (best_template->template_index -
                         frame_dependency_structure_->templates.begin() +
                         frame_dependency_structure_->structure_id) %
                        kMaxTemplates;
  rtc::BitBufferWriter bit_writer(raw_data.data(), raw_data.size());
  bool has_extended_fields =
      best_template->extra_size_bits > 0 || descriptor.has_structure_attached;
  // Write mandataory fields.
  uint8_t first_byte =
      has_extended_fields ? kExtendedFieldsIndicator : template_id;
  if (descriptor.first_packet_in_frame)
    first_byte |= 0x80;
  if (descriptor.last_packet_in_frame)
    first_byte |= 0x40;
  if (!bit_writer.WriteUInt8(first_byte))
    return false;
  if (!bit_writer.WriteUInt16(descriptor.frame_number))
    return false;

  if (!has_extended_fields) {
    return true;
  }
  // Extended fields.
  uint32_t extended_fields = (template_id << 4);
  extended_fields |= (descriptor.has_structure_attached << 3);
  extended_fields |= (best_template->need_custom_dtis << 2);
  extended_fields |= (best_template->need_custom_fdiffs << 1);
  extended_fields |= (best_template->need_custom_chains << 0);
  if (!bit_writer.WriteBits(extended_fields, 10))
    return false;

  if (descriptor.has_structure_attached &&
      !WriteTemplateDependencyStructure(
          &bit_writer, frame_dependency_structure_->structure_id,
          *frame_dependency_structure_))
    return false;
  if (best_template->need_custom_dtis) {
    if (!WriteFrameDtis(descriptor.frame_dependencies.decode_target_indications,
                        &bit_writer))
      return false;
  }
  if (best_template->need_custom_fdiffs) {
    if (!WriteFrameFdiffs(descriptor.frame_dependencies.frame_diffs,
                          &bit_writer))
      return false;
  }
  if (best_template->need_custom_chains) {
    if (!WriterFrameChains(descriptor.frame_dependencies.chain_diffs,
                           &bit_writer))
      return false;
  }
  return true;
}

bool RtpDependencyDescriptorWriter::SetStructure(
    const FrameDependencyStructure& structure) {
  if (!IsValid(structure)) {
    return false;
  }
  int template_id_offset = 0;
  if (frame_dependency_structure_ == absl::nullopt &&
      structure.structure_id >= 0) {
    template_id_offset = structure.structure_id % kMaxTemplates;
  }
  if (frame_dependency_structure_) {
    if (*frame_dependency_structure_ == structure) {
      // No need to update anything.
      return true;
    }

    if (frame_dependency_structure_->templates.size() +
            structure.templates.size() >
        kMaxTemplates) {
      // Reject new structure if template ids it uses overlaps with the old
      // template ids.
      return false;
    }

    template_id_offset = (frame_dependency_structure_->structure_id +
                          frame_dependency_structure_->templates.size()) %
                         kMaxTemplates;
  }
  structure_size_bits_ = StructureSizeBits(structure);
  frame_dependency_structure_ = structure;
  frame_dependency_structure_->structure_id = template_id_offset;
  return true;
}

absl::optional<RtpDependencyDescriptorWriter::TemplateMatch>
RtpDependencyDescriptorWriter::FindBestTemplate(
    const DependencyDescriptor& descriptor) const {
  if (!frame_dependency_structure_) {
    return absl::nullopt;
  }
  if (static_cast<int>(
          descriptor.frame_dependencies.decode_target_indications.size()) !=
      frame_dependency_structure_->num_decode_targets) {
    return absl::nullopt;
  }
  // Find range of templates with matching spatial/temporal id.
  auto same_layer = [&](const FrameDependencyTemplate& frame_template) {
    return descriptor.frame_dependencies.spatial_id ==
               frame_template.spatial_id &&
           descriptor.frame_dependencies.temporal_id ==
               frame_template.temporal_id;
  };
  const auto& templates = frame_dependency_structure_->templates;
  auto first = absl::c_find_if(templates, same_layer);
  if (first == templates.end()) {
    return absl::nullopt;
  }
  auto last = std::find_if_not(first, templates.end(), same_layer);

  TemplateMatch best_match = AdditionalSizeBits(descriptor, first);
  for (++first; first != last; ++first) {
    TemplateMatch match = AdditionalSizeBits(descriptor, first);
    if (match.extra_size_bits < best_match.extra_size_bits) {
      best_match = match;
    }
  }
  return best_match;
}

}  // namespace webrtc
