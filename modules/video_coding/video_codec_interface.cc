/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>

#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

FrameEncodingInfo::FrameEncodingInfo() = default;
FrameEncodingInfo::FrameEncodingInfo(const FrameEncodingInfo&) = default;
FrameEncodingInfo::~FrameEncodingInfo() = default;

GenericFrameInfo::GenericFrameInfo() = default;
GenericFrameInfo::GenericFrameInfo(const GenericFrameInfo&) = default;
GenericFrameInfo::~GenericFrameInfo() = default;

std::string GenericFrameInfo::ToString() const {
  std::stringstream res;
  res << "TID:" << temporal_id << " "
      << "SID:" << spatial_id << " ";

  res << "DTIs:{ ";
  for (auto indication : operating_points) {
    switch (static_cast<int>(indication)) {
      case 0:
        res << "- ";
        continue;
      case 1:
        res << "D ";
        continue;
      case 2:
        res << "S ";
        continue;
      case 3:
        res << "R ";
        continue;
    }
  }
  res << "} ";

  res << "FDIFFS:{ ";
  for (int d : frame_diffs)
    res << d << " ";
  res << "} ";

  return res.str();
}

TemplateStructure::TemplateStructure() = default;
TemplateStructure::TemplateStructure(const TemplateStructure&) = default;
TemplateStructure::~TemplateStructure() = default;

TemplateStructure::Template::Template() = default;
TemplateStructure::Template::Template(const Template&) = default;
TemplateStructure::Template::~Template() = default;

CodecSpecificInfo::CodecSpecificInfo() : codecType(kVideoCodecGeneric) {
  memset(&codecSpecific, 0, sizeof(codecSpecific));
}

CodecSpecificInfo::~CodecSpecificInfo() = default;

}  // namespace webrtc
