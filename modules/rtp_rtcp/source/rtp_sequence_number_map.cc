/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"

#include <iterator>

#include "rtc_base/numerics/sequence_number_util.h"

namespace webrtc {
using Info = RtpSequenceNumberMap::Info;

RtpSequenceNumberMap::RtpSequenceNumberMap() = default;

RtpSequenceNumberMap::~RtpSequenceNumberMap() = default;

void RtpSequenceNumberMap::Insert(uint16_t sequence_number, Info info) {
  rtc::CritScope cs(&cs_);

  if (associations_.empty()) {
    associations_.emplace_back(sequence_number, info);
    return;
  }

  const Association new_association(sequence_number, info);
  if (!AheadOf(new_association.sequence_number,
               associations_.front().sequence_number)) {
    auto erase_to = std::next(associations_.begin());
    // TODO: !!! Binary search.
    while (
        erase_to != associations_.end() &&
        !AheadOf(new_association.sequence_number, erase_to->sequence_number)) {
      ++erase_to;
    }
    associations_.erase(associations_.begin(), erase_to);
  }

  associations_.emplace_back(sequence_number, info);

  RTC_DCHECK(associations_.size() == 1 ||
             AheadOf(associations_.back().sequence_number,
                     associations_.front().sequence_number));
}

absl::optional<Info> RtpSequenceNumberMap::Get(uint16_t sequence_number) const {
  rtc::CritScope cs(&cs_);

  // TODO: !!!
  // TODO(eladalon): std::deque provides O(1) random-access, and the
  // elements are sorted, so we should use binary search.
  for (auto association : associations_) {
    if (association.sequence_number == sequence_number) {
      return association.info;
    }
  }

  return absl::nullopt;
}

}  // namespace webrtc
