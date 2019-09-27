/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {

TEST(TransceiverIssue, SmokeTest) {
  PeerScenario s;
  auto caller = s.CreateClient(PeerScenarioClient::Config());
  auto callee = s.CreateClient(PeerScenarioClient::Config());
  caller->CreateVideo("VIDEO1", PeerScenarioClient::VideoSendTrackConfig());
  caller->CreateVideo("VIDEO2", PeerScenarioClient::VideoSendTrackConfig());
  auto link_builder = s.net()->NodeBuilder().delay_ms(100).capacity_kbps(600);
  s.SimpleConnection(caller, callee, {link_builder.Build().node},
                     {link_builder.Build().node});
  s.ProcessMessages(TimeDelta::seconds(2));
}

}  // namespace test
}  // namespace webrtc
