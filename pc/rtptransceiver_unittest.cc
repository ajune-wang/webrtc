/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests for |RtpTransceiver|.

#include "pc/rtptransceiver.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/mock_basechannel.h"

namespace webrtc {

// Test class for |RtpTransceiver|.
class RtpTransceiverTest : public testing::Test {};

// Checks that a channel cannot be set on a stopped |RtpTransceiver|.
TEST_F(RtpTransceiverTest, CannotSetChannelOnStoppedTransceiver) {
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
  cricket::MockBaseChannel channel1;
  sigslot::signal1<cricket::BaseChannelInterface*> signal;
  EXPECT_CALL(channel1, media_type())
      .WillRepeatedly(testing::Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(channel1, SignalFirstPacketReceived())
      .WillRepeatedly(testing::ReturnRef(signal));

  transceiver.SetChannel(&channel1);
  ASSERT_EQ(&channel1, transceiver.channel());

  // Stop the transceiver.
  transceiver.Stop();
  ASSERT_EQ(&channel1, transceiver.channel());

  // Set the channel to |nullptr|.
  transceiver.SetChannel(nullptr);
  ASSERT_EQ(nullptr, transceiver.channel());

  cricket::MockBaseChannel channel2;
  EXPECT_CALL(channel2, media_type())
      .WillRepeatedly(testing::Return(cricket::MediaType::MEDIA_TYPE_AUDIO));

  // Channel can no longer be set, so this call should be a no-op.
  transceiver.SetChannel(&channel2);
  ASSERT_EQ(nullptr, transceiver.channel());
}

}  // namespace webrtc
