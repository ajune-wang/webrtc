/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_SIZE_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_SIZE_H_

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"

namespace webrtc {

struct RtpExtensionSize {
  absl::string_view uri;
  int value_size;
  // Non-volatile extensions can be expected on all packets, if registered.
  // Volatile ones, such as VideoContentTypeExtension which is only set on
  // key-frames, are removed to simplify overhead calculations at the expense of
  // some accuracy.
  bool is_volatile = false;
};

// Calculates rtp header extension size in bytes assuming packet contain
// all `extensions` with provided `value_size`.
// Counts only extensions present among `registered_extensions`.
int RtpHeaderExtensionSize(rtc::ArrayView<const RtpExtensionSize> extensions,
                           const RtpHeaderExtensionMap& registered_extensions);

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_SIZE_H_
