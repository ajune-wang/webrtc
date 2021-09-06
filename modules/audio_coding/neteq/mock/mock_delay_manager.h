/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_DELAY_MANAGER_H_
#define MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_DELAY_MANAGER_H_

#include <memory>
#include <utility>

#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/delay_manager.h"
#include "test/gmock.h"

namespace webrtc {

class MockDelayOptimizer : public DelayOptimizer {
  MOCK_METHOD(void, Update, (int));
  MOCK_METHOD(absl::optional<int>, GetOptimalDelayMs, (), (const));
  MOCK_METHOD(void, Reset, ());
};

class MockDelayManager : public DelayManager {
 public:
  // TODO(jakobi): Make delay manager pure virtual interface and remove this
  // constructor.
  MockDelayManager(size_t max_packets_in_buffer,
                   int base_minimum_delay_ms,
                   std::unique_ptr<DelayOptimizer> underrun_optimizer,
                   int max_history_ms,
                   const TickTimer* tick_timer)
      : DelayManager(max_packets_in_buffer,
                     base_minimum_delay_ms,
                     std::move(underrun_optimizer),
                     max_history_ms,
                     tick_timer) {}
  MOCK_METHOD(int, TargetDelayMs, (), (const));
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_DELAY_MANAGER_H_
