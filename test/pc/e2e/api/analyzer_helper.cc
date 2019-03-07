/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/api/analyzer_helper.h"

namespace webrtc {
namespace test {

void AnalyzerHelper::AddTrackToStreamMapping(const std::string& track_id,
                                             const std::string& stream_label) {
  rtc::CritScope crit(&lock_);
  track_to_stream_map_.insert({track_id, stream_label});
}

const std::string& AnalyzerHelper::GetStreamLabelFromTrackId(
    const std::string track_id) const {
  rtc::CritScope crit(&lock_);
  auto track_to_stream_pair = track_to_stream_map_.find(track_id);
  RTC_CHECK(track_to_stream_pair != track_to_stream_map_.end());
  return track_to_stream_pair->second;
}

}  // namespace test
}  // namespace webrtc
