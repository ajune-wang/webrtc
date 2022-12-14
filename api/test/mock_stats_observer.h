/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_STATSOVSERVER_H_
#define API_TEST_MOCK_STATSOVSERVER_H_

#include "api/legacy_stats_types.h"
#include "api/peer_connection_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockStatsObserver : public MOCK_CLASS(webrtc::StatsObserver) {
 public:
  MOCK_METHOD(void,
              OnComplete,
              (const webrtc::StatsReports& legacy_stats_reports),
              (override));
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_STATSOVSERVER_H_
