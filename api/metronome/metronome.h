/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/system/rtc_export.h"

#ifndef API_METRONOME_METRONOME_H_
#define API_METRONOME_METRONOME_H_

namespace webrtc {

// This is an experimental class and may be deleted at any time.
class RTC_EXPORT Metronome {
 public:
  class RTC_EXPORT TickListener {
   public:
    virtual ~TickListener() = default;

    // Is run each time the metronome ticks, and is run on the |OnTickTaskQueue|
    virtual void OnTick() = 0;

    // The task queue that |OnTick| will run on. Must not be null.
    virtual TaskQueueBase* OnTickTaskQueue() = 0;
  };

  virtual ~Metronome() = default;

  virtual void AddListener(TickListener* listener,
                           TaskQueueBase* task_queue) = 0;
  virtual void RemoveListener(TickListener* listener) = 0;
  virtual TimeDelta TickPeriod() const = 0;
};

}  // namespace webrtc

#endif  // API_METRONOME_METRONOME_H_
