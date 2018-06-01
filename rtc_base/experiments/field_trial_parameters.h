/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARAMETERS_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARAMETERS_H_

#include <stdint.h>
#include <string>
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

// An empty string is considered true. This is useful since the bool can be
// enabled just by including the key example: ",Enabled," -> true.
extern template class FieldTrialParameter<bool>;
extern template class FieldTrialParameter<double>;
extern template class FieldTrialParameter<int64_t>;
extern template class FieldTrialParameter<std::string>;

extern template class FieldTrialParameter<rtc::Optional<double>>;
extern template class FieldTrialParameter<rtc::Optional<int64_t>>;
extern template class FieldTrialParameter<rtc::Optional<bool>>;
}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_PARAMETERS_H_
