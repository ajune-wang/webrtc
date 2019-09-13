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

#include "api/array_view.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "rtc_base/bit_buffer.h"

namespace webrtc {
// Keeps and updates state required to deserialize DependencyDescriptor
// rtp header extension.
class RtpDependencyDescriptorReader {
 public:
  // Parses the dependency descriptor. Returns false on failure.
  // Updates frame dependency structures if parsed descriptor has a new one.
  // Doesn't update own state when Parse fails.
  bool Parse(rtc::ArrayView<const uint8_t> raw_data,
             DependencyDescriptor* descriptor);

  // Returns latest valid parsed structure or nullptr if none was parsed so far.
  const FrameDependencyStructure* GetStructure() const {
    return frame_dependency_structure_.get();
  }

 private:
  // Values that are needed while reading the descriptor, but can be discarded
  // when reading is complete.
  struct ReadingState;

  static bool ReadMandatoryFields(rtc::BitBuffer* buffer,
                                  ReadingState* reading_state,
                                  DependencyDescriptor* descriptor);
  static bool ReadTemplateChains(rtc::BitBuffer* buffer,
                                 FrameDependencyStructure* structure);

  // Reader shouldn't update own state (structure) unless read was successful.
  // Otherwise there can be subtle situation where some packets look discarded
  // yet update state to errorness.
  bool Read(rtc::BitBuffer* bit_reader,
            ReadingState* reading_state,
            DependencyDescriptor* descriptor) const;

  bool ReadExtendedFields(rtc::BitBuffer* buffer,
                          ReadingState* reading_state,
                          DependencyDescriptor* descriptor) const;
  bool ReadTemplateDependencyStructure(rtc::BitBuffer* buffer,
                                       ReadingState* reading_state) const;
  bool ReadFrameDependencyDefinition(rtc::BitBuffer* buffer,
                                     const ReadingState& reading_state,
                                     DependencyDescriptor* descriptor) const;

  // Size of the last read frame dependency structure (excluding
  // template_id_offset field).
  int structure_size_bits_ = 0;
  std::unique_ptr<FrameDependencyStructure> frame_dependency_structure_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_READER_H_
