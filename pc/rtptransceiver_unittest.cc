/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests for |RtpTransceiver|

#include "pc/rtptransceiver.h"
#include "rtc_base/gunit.h"
#include "test/mock_basechannel.h"

namespace webrtc {

class RtpTransceiverTest : public testing::Test {};

TEST_F(RtpTransceiverTest, CannotSetStreamOnStoppedTransceiver) {
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
  MockBaseChannel channel1(cricket::MediaType::MEDIA_TYPE_AUDIO);
  transceiver.SetChannel(&channel1);
  ASSERT_EQ(&channel1, transceiver.channel());

  // stop the transceiver
  transceiver.Stop();
  ASSERT_EQ(&channel1, transceiver.channel());

  // reset the channel
  transceiver.SetChannel(nullptr);
  ASSERT_EQ(nullptr, transceiver.channel());

  // channel can no longer be set
  MockBaseChannel channel2(cricket::MediaType::MEDIA_TYPE_AUDIO);

  // should not be able to reset the channel
  transceiver.SetChannel(&channel2);
  ASSERT_EQ(nullptr, transceiver.channel());
}

}  // namespace webrtc
