/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_METRONOME_TEST_FAKE_METRONOME_H_
#define API_METRONOME_TEST_FAKE_METRONOME_H_

#include <set>

#include "api/metronome/metronome.h"
#include "api/units/time_delta.h"

namespace webrtc {

class FakeMetronome : public Metronome {
 public:
  explicit FakeMetronome(TimeDelta tick_period);

  // Forces all TickListeners to run `OnTick`.
  void Tick();

  // Metronome implementation.
  void AddListener(TickListener* listener) override;
  void RemoveListener(TickListener* listener) override;
  TimeDelta TickPeriod() const override;

 private:
  const TimeDelta tick_period_;
  std::set<TickListener*> listeners_;
};

}  // namespace webrtc

#endif  // API_METRONOME_TEST_FAKE_METRONOME_H_
