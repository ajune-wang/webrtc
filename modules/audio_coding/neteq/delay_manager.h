/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DELAY_MANAGER_H_
#define MODULES_AUDIO_CODING_NETEQ_DELAY_MANAGER_H_

#include <string.h>  // Provide access to size_t.

#include <deque>
#include <memory>

#include "absl/types/optional.h"
#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/relative_arrival_delay_tracker.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class DelayOptimizer {
 public:
  virtual ~DelayOptimizer() = default;
  virtual void Update(int relative_delay_ms) = 0;
  virtual absl::optional<int> GetOptimalDelayMs() const = 0;
  virtual void Reset() = 0;
};

std::unique_ptr<DelayOptimizer> CreateUnderrunOptimizer(
    const TickTimer* tick_timer,
    int histogram_quantile,
    int forget_factor,
    absl::optional<int> start_forget_weight,
    absl::optional<int> resample_interval_ms);

class DelayManager {
 public:
  DelayManager(int max_packets_in_buffer,
               int base_minimum_delay_ms,
               std::unique_ptr<DelayOptimizer> underrun_optimizer,
               int max_history_ms,
               const TickTimer* tick_timer);

  // Create a DelayManager object. Notify the delay manager that the packet
  // buffer can hold no more than `max_packets_in_buffer` packets (i.e., this
  // is the number of packet slots in the buffer) and that the target delay
  // should be greater than or equal to `base_minimum_delay_ms`. Supply a
  // PeakDetector object to the DelayManager.
  static std::unique_ptr<DelayManager> Create(int max_packets_in_buffer,
                                              int base_minimum_delay_ms,
                                              const TickTimer* tick_timer);

  virtual ~DelayManager();

  // Updates the delay manager with a new incoming packet, with `timestamp` from
  // the RTP header. This updates the statistics and a new target buffer level
  // is calculated. Returns the relative delay if it can be calculated. If
  // `reset` is true, restarts the relative arrival delay calculation from this
  // packet.
  virtual absl::optional<int> Update(uint32_t timestamp,
                                     int sample_rate_hz,
                                     bool reset = false);

  // Resets all state.
  virtual void Reset();

  // Gets the target buffer level in milliseconds.
  virtual int TargetDelayMs() const;

  // Notifies the DelayManager of how much audio data is carried in each packet.
  virtual int SetPacketAudioLength(int length_ms);

  // Accessors and mutators.
  // Assuming `delay` is in valid range.
  virtual bool SetMinimumDelay(int delay_ms);
  virtual bool SetMaximumDelay(int delay_ms);
  virtual bool SetBaseMinimumDelay(int delay_ms);
  virtual int GetBaseMinimumDelay() const;

  // These accessors are only intended for testing purposes.
  int effective_minimum_delay_ms_for_test() const {
    return effective_minimum_delay_ms_;
  }

 private:
  // Provides value which minimum delay can't exceed based on current buffer
  // size and given `maximum_delay_ms_`. Lower bound is a constant 0.
  int MinimumDelayUpperBound() const;

  // Updates `effective_minimum_delay_ms_` delay based on current
  // `minimum_delay_ms_`, `base_minimum_delay_ms_` and `maximum_delay_ms_`
  // and buffer size.
  void UpdateEffectiveMinimumDelay();

  // Makes sure that `delay_ms` is less than maximum delay, if any maximum
  // is set. Also, if possible check `delay_ms` to be less than 75% of
  // `max_packets_in_buffer_`.
  bool IsValidMinimumDelay(int delay_ms) const;

  bool IsValidBaseMinimumDelay(int delay_ms) const;

  // TODO(jakobi): set maximum buffer delay instead of number of packets.
  const int max_packets_in_buffer_;
  std::unique_ptr<DelayOptimizer> underrun_optimizer_;
  RelativeArrivalDelayTracker relative_arrival_delay_tracker_;

  int base_minimum_delay_ms_;
  int effective_minimum_delay_ms_;  // Used as lower bound for target delay.
  int minimum_delay_ms_;            // Externally set minimum delay.
  int maximum_delay_ms_;            // Externally set maximum allowed delay.

  int packet_len_ms_ = 0;
  int target_level_ms_;       // Currently preferred buffer level.

  RTC_DISALLOW_COPY_AND_ASSIGN(DelayManager);
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_DELAY_MANAGER_H_
