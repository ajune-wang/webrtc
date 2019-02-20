/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_STATS_POLLER_H_
#define TEST_PC_E2E_STATS_POLLER_H_

#include <string>
#include <utility>
#include <vector>

#include "api/peer_connection_interface.h"
#include "test/pc/e2e/api/stats_observer_interface.h"
#include "test/pc/e2e/test_peer.h"

namespace webrtc {
namespace test {

// Helper class that will notify all the webrtc::test::StatsObserverInterface
// objects subscribed.
class InternalStatsObserver : public StatsObserver {
 public:
  InternalStatsObserver(std::string pc_label,
                        std::vector<StatsObserverInterface*> observers)
      : pc_label_(pc_label), observers_(observers) {}

  void OnComplete(const StatsReports& reports) override {
    RTC_LOG(INFO) << "Polling for " << pc_label_ << " completed.";
    for (auto observer : observers_) {
      observer->OnStatsReports(pc_label_, reports);
    }
  }

 private:
  std::string pc_label_;
  std::vector<StatsObserverInterface*> observers_;
};

// Helper class to invoke GetStats on a PeerConnection by passing a
// webrtc::StatsObserver that will notify all the
// webrtc::test::StatsObserverInterface subscribed.
class StatsPoller {
 public:
  StatsPoller(std::vector<StatsObserverInterface*> observers,
              std::vector<std::pair<std::string, TestPeer*>>);

  void PollStatsAndNotifyObservers() const;

 private:
  std::vector<std::pair<rtc::scoped_refptr<InternalStatsObserver>, TestPeer*>>
      peers_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_STATS_POLLER_H_
