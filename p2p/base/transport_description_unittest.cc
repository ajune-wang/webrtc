/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transport_description.h"
#include "test/gtest.h"

using webrtc::RTCErrorType;

namespace cricket {

TEST(IceParameters, SuccessfulParse) {
  auto result =
      IceParameters("ufrag", "22+characters+long+pwd", /* renomination= */ true)
          .Validate();
  ASSERT_TRUE(result.ok());
}

TEST(IceParameters, FailedParseShortUfrag) {
  auto result =
      IceParameters("3ch", "22+characters+long+pwd", /* renomination= */ true)
          .Validate();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.type());
}

TEST(IceParameters, FailedParseLongUfrag) {
  std::string ufrag(257, '+');
  auto result =
      IceParameters(ufrag, "22+characters+long+pwd", /* renomination= */ true)
          .Validate();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.type());
}

TEST(IceParameters, FailedParseShortPwd) {
  auto result =
      IceParameters("ufrag", "21+character+long+pwd", /* renomination= */ true)
          .Validate();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.type());
}

TEST(IceParameters, FailedParseLongPwd) {
  std::string pwd(257, '+');
  auto result =
      IceParameters("ufrag", pwd, /* renomination= */ true).Validate();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.type());
}

TEST(IceParameters, FailedParseBadUfragChar) {
  auto result = IceParameters("ufrag\r\n", "22+characters+long+pwd",
                              /* renomination= */ true)
                    .Validate();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.type());
}

TEST(IceParameters, FailedParseBadPwdChar) {
  auto result = IceParameters("ufrag", "22+characters+long+pwd\r\n",
                              /* renomination= */ true)
                    .Validate();
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.type());
}

}  // namespace cricket
