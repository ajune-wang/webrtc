/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_TEST_CONTROLLER_PRINTER_H_
#define MODULES_CONGESTION_CONTROLLER_TEST_CONTROLLER_PRINTER_H_

#include <memory>

#include "api/units/timestamp.h"

namespace webrtc {

class ControlStatePrinter {
 public:
  virtual ~ControlStatePrinter() = default;
  virtual void PrintState(const Timestamp at_time) = 0;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_TEST_CONTROLLER_PRINTER_H_
