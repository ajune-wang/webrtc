/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_HDR_METADATA_H_
#define API_VIDEO_HDR_METADATA_H_

namespace webrtc {

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
// Replicates the HdrMetadata struct defined in
// chromium/src/media/base/hdr_metadata.h
struct HdrMetadata {
  // mastering_metadata ported yet since it won't fit into the one-byte header
  // extension.
  // MasteringMetadata mastering_metadata;
  // Max content light level (CLL), i.e. maximum brightness level present in the
  // stream), in nits.
  unsigned max_content_light_level = 0;
  // Max frame-average light level (FALL), i.e. maximum average brightness of
  // the brightest frame in the stream), in nits.
  unsigned max_frame_average_light_level = 0;

  // Offsets of the fields in the RTP header extension, counting from the first
  // byte after the one-byte header.
  static constexpr uint8_t kMaxContentLightLevelOffset = 0;
  static constexpr uint8_t kMaxFrameAverageLightLevelOffset = 4;
};

}  // namespace webrtc

#endif  // API_VIDEO_HDR_METADATA_H_
