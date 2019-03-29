/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rename_me.h"

#include <iterator>

namespace webrtc {
using Value = RecoveryRequestAdapter::Value;

RecoveryRequestAdapter::RecoveryRequestAdapter() = default;

RecoveryRequestAdapter::~RecoveryRequestAdapter() = default;

void RecoveryRequestAdapter::RecordNewAssociation(uint16_t key, Value value) {
  rtc::CritScope cs(&cs_);

  if (associations_.empty()) {
    associations_.emplace_back(key, value);
    return;
  }

  const Association new_association(key, value);
  if (!AheadOf(new_association.key, associations_.front().key)) {
    auto erase_to = std::next(associations_.begin());
    while (erase_to != associations_.end() &&
           !AheadOf(new_association.key, erase_to->key)) {
      ++erase_to;
    }
    associations_.erase(associations_.begin(), erase_to);
  }

  associations_.emplace_back(key, value);

  RTC_DCHECK(associations_.size() == 1 ||
             AheadOf(associations_.back().key, associations_.front().key));
}

absl::optional<Value> RecoveryRequestAdapter::GetValue(uint16_t key) const {
  rtc::CritScope cs(&cs_);

  // TODO(eladalon): std::deque provides O(1) random-access, and the
  // elements are sorted, so we should use binary search.
  for (auto association : associations_) {
    if (association.key == key) {
      return association.value;
    }
  }

  return absl::nullopt;
}

}  // namespace webrtc
