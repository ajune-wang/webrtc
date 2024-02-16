/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/candidate.h"

#include <string>

#include "p2p/base/p2p_constants.h"
#include "rtc_base/gunit.h"

namespace cricket {

TEST(CandidateTest, Id) {
  Candidate c;
  EXPECT_EQ(c.id().size(), 8u);
  std::string new_id = "12345678";
  c.set_id(new_id);
  EXPECT_EQ(new_id, c.id());
}

TEST(CandidateTest, Component) {
  Candidate c;
  EXPECT_EQ(c.component(), 0);
  c.set_component(ICE_CANDIDATE_COMPONENT_DEFAULT);
  EXPECT_EQ(c.component(), ICE_CANDIDATE_COMPONENT_DEFAULT);
}

TEST(CandidateTest, TypeName) {
  Candidate c;
  // The `type_name()` property defaults to "host".
  EXPECT_EQ(c.type_name(), "host");
  EXPECT_EQ(c.type(), LOCAL_PORT_TYPE);

  c.set_type(STUN_PORT_TYPE);
  EXPECT_EQ(c.type_name(), "srflx");
  EXPECT_EQ(c.type(), STUN_PORT_TYPE);

  c.set_type(PRFLX_PORT_TYPE);
  EXPECT_EQ(c.type_name(), "prflx");
  EXPECT_EQ(c.type(), PRFLX_PORT_TYPE);

  c.set_type(RELAY_PORT_TYPE);
  EXPECT_EQ(c.type_name(), "relay");
  EXPECT_EQ(c.type(), RELAY_PORT_TYPE);
  {
    // Even though we're using `ABSL_ATTRIBUTE_LIFETIME_BOUND`, that doesn't
    // catch cases like this where a view to a string whose scope is narrower
    // than that of `c`. The type should still be set and the internally stored
    // type should not point to the
    std::string out_of_scope_type(PRFLX_PORT_TYPE);
    c.set_type(out_of_scope_type);
    EXPECT_NE(out_of_scope_type.data(), c.type_name().data());
  }
  // Even though `out_of_scope_type` is now gone, the type should still be
  // correct.
  EXPECT_EQ(c.type_name(), "prflx");
}

}  // namespace cricket
