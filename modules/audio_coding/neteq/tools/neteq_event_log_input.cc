/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_event_log_input.h"

#include <limits>
#include <memory>

#include "absl/strings/string_view.h"
#include "modules/audio_coding/neteq/tools/rtc_event_log_source.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

std::unique_ptr<PacketSource> CreateFromFile(
    absl::string_view file_name,
    absl::optional<uint32_t> ssrc_filter) {
  std::unique_ptr<RtcEventLogSource> event_log_src =
      RtcEventLogSource::CreateFromFile(file_name, ssrc_filter);
  if (!event_log_src) {
    return nullptr;
  }
  return event_log_src;
}

std::unique_ptr<PacketSource> CreateFromString(
    absl::string_view file_contents,
    absl::optional<uint32_t> ssrc_filter) {
  auto event_log_src =
      RtcEventLogSource::CreateFromString(file_contents, ssrc_filter);
  if (!event_log_src) {
    return nullptr;
  }
  return event_log_src;
}

}  // namespace test
}  // namespace webrtc
