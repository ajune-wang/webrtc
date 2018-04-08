/*  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_EXPAND_UMA_LOGGER_H_
#define MODULES_AUDIO_CODING_NETEQ_EXPAND_UMA_LOGGER_H_

#include <memory>
#include <string>

#include "api/optional.h"
#include "modules/audio_coding/neteq/tick_timer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ExpandUmaLogger {
 public:
  ExpandUmaLogger(std::string uma_name,
                  int logging_period_ms,
                  const TickTimer* tick_timer);

  ~ExpandUmaLogger();

  void UpdateSampleCounter(uint64_t value, int sample_rate_hz);

 private:
  const std::string uma_name_;
  const int logging_period_ms_;
  const TickTimer& tick_timer_;
  std::unique_ptr<TickTimer::Countdown> timer_;
  rtc::Optional<uint64_t> last_logged_value_;
  uint64_t last_value_ = 0;
  int sample_rate_hz_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(ExpandUmaLogger);
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_EXPAND_UMA_LOGGER_H_
