/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/field_trials_registry.h"

#include <string>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "experiments/registered_field_trials.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"

namespace webrtc {

std::string FieldTrialsRegistry::Lookup(absl::string_view key) const {
#if WEBRTC_STRICT_FIELD_TRIALS
  RTC_DCHECK(absl::c_any_of(kRegisteredFieldTrials,
                            [&](absl::string_view k) { return k == key; }) ||
             test_keys_.contains(key))
      << key << " is not registered.";
#endif
  return GetValue(key);
}

void FieldTrialsRegistry::RegisterKeysForTesting(
    rtc::ArrayView<const absl::string_view> keys) {
  for (const absl::string_view key : keys) {
    test_keys_.insert(std::string(key));
  }
}

}  // namespace webrtc
