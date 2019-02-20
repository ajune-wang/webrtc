/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/stats_poller.h"

#include <utility>

#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

StatsPoller::StatsPoller(std::vector<StatsObserverInterface*> observers,
                         std::vector<std::pair<std::string, TestPeer*>> peers) {
  for (auto& peer : peers) {
    peers_.push_back(std::make_pair(
        new rtc::RefCountedObject<InternalStatsObserver>(peer.first, observers),
        peer.second));
  }
}

void StatsPoller::PollStatsAndNotifyObservers() const {
  for (auto& peer : peers_) {
    RTC_LOG(INFO) << "Polling " << peer.first << " stats.";
    peer.second->pc()->GetStats(
        peer.first, nullptr,
        webrtc::PeerConnectionInterface::StatsOutputLevel::
            kStatsOutputLevelStandard);
  }
}

}  // namespace test
}  // namespace webrtc
