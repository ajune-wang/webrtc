/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PEER_SCENARIO_PEER_SCENARIO_H_
#define TEST_PEER_SCENARIO_PEER_SCENARIO_H_

#include <list>
#include <memory>
#include <vector>

#include "test/network/network_emulation_manager.h"
#include "test/peer_scenario/peer_scenario_client.h"
#include "test/peer_scenario/signaling_route.h"
#include "test/scenario/stats_collection.h"
#include "test/scenario/video_frame_matcher.h"

namespace webrtc {
namespace test {

class PeerScenario {
 public:
  PeerScenario();
  NetworkEmulationManagerImpl* net() { return net_.get(); }
  rtc::Thread* thread() { return signaling_thread_; }

  PeerScenarioClient* CreateClient(PeerScenarioClient::Config config);

  SignalingRoute ConnectSignaling(PeerScenarioClient* caller,
                                  PeerScenarioClient* callee,
                                  std::vector<EmulatedNetworkNode*> send_link,
                                  std::vector<EmulatedNetworkNode*> ret_link);

  void SimpleConnection(PeerScenarioClient* caller,
                        PeerScenarioClient* callee,
                        std::vector<EmulatedNetworkNode*> send_link,
                        std::vector<EmulatedNetworkNode*> ret_link);

  void AttachVideoQualityAnalyzer(Clock* capture_clock,
                                  VideoQualityAnalyzer* analyzer,
                                  VideoTrackInterface* send_track,
                                  PeerScenarioClient* receiver);

  bool Wait(rtc::Event* event, TimeDelta max_duration = TimeDelta::seconds(5));
  void Sleep(TimeDelta duration);

 private:
  struct PeerVideoQualityPair {
   public:
    PeerVideoQualityPair(Clock* capture_clock, VideoQualityAnalyzer* analyzer)
        : matcher_({analyzer->Handler()}),
          capture_tap_(capture_clock, &matcher_),
          decode_tap_(capture_clock, &matcher_, 0) {}
    VideoFrameMatcher matcher_;
    CapturedFrameTap capture_tap_;
    DecodedFrameTap decode_tap_;
  };

  rtc::Thread* const signaling_thread_;
  std::vector<std::unique_ptr<PeerVideoQualityPair>> video_quality_pairs_;
  std::unique_ptr<NetworkEmulationManagerImpl> net_;
  std::vector<std::unique_ptr<PeerScenarioClient>> peer_clients_;
};

}  // namespace test
}  // namespace webrtc
#endif  // TEST_PEER_SCENARIO_PEER_SCENARIO_H_
