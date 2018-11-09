/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_BASECHANNEL_H_
#define PC_TEST_MOCK_BASECHANNEL_H_

#include <string>

#include "pc/channel.h"
#include "test/gmock.h"

namespace webrtc {

// Mock class for BaseChannel.
// Use this class in unit tests to avoid dependecy on a specific
// implementation of BaseChannel
class MockBaseChannel : public cricket::BaseChannel {
 public:
  // Create a MockBaseChannel.
  // This channel is for the specified |media_type|.
  // The |content| is optional
  MockBaseChannel(cricket::MediaType media_type,
                  const std::string& content = "content")
      : BaseChannel(rtc::Thread::Current(),
                    nullptr,
                    nullptr,
                    nullptr,
                    content,
                    false,
                    CryptoOptions()) {
    // Set up |media_type()| to return the |media_type| argument
    EXPECT_CALL(*this, media_type())
        .WillRepeatedly(testing::Return(media_type));
  }

  // Implementation of pure virtual members in BaseChannel
  MOCK_METHOD0(media_type, cricket::MediaType());
  MOCK_METHOD0(UpdateMediaSendRecvState_w, void());
  MOCK_METHOD3(SetLocalContent_w,
               bool(const cricket::MediaContentDescription*,
                    SdpType,
                    std::string*));
  MOCK_METHOD3(SetRemoteContent_w,
               bool(const cricket::MediaContentDescription*,
                    SdpType,
                    std::string*));
};

}  // namespace webrtc

#endif  // PC_TEST_MOCK_BASECHANNEL_H_
