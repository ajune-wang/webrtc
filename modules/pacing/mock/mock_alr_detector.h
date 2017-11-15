/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_MOCK_MOCK_ALR_DETECTOR_H_
#define MODULES_PACING_MOCK_MOCK_ALR_DETECTOR_H_

#include "modules/pacing/alr_detector.h"
#include "test/gmock.h"

namespace webrtc {

class MockAlrDetector : public AlrDetector {
 public:
  MockAlrDetector() {}
  MOCK_CONST_METHOD0(GetApplicationLimitedRegionStartTime,
                     rtc::Optional<int64_t>());
};
}  // namespace webrtc

#endif  // MODULES_PACING_MOCK_MOCK_ALR_DETECTOR_H_
