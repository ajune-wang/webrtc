/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {
struct DummyConfig {
  bool enabled = false;
  double factor = 0.5;
  int retries = 5;
  bool ping = 0;
  std::string hash = "a80";
  absl::optional<TimeDelta> duration;
  absl::optional<TimeDelta> latency = TimeDelta::ms(100);
  static const StructParametersParser<DummyConfig>& Parser();
};

const StructParametersParser<DummyConfig>& DummyConfig::Parser() {
  // The empty comments ensures that each pair is on a separate line.
  auto* parser = CreateStructParametersParser([](DummyConfig* c) {
    return StructMembers<DummyConfig>(  //
        "e", &c->enabled,               //
        "f", &c->factor,                //
        "r", &c->retries,               //
        "p", &c->ping,                  //
        "h", &c->hash,                  //
        "d", &c->duration,              //
        "l", &c->latency);
  });
  return *parser;
}
}  // namespace

TEST(StructParametersParserTest, ParsesValidParameters) {
  DummyConfig exp =
      DummyConfig::Parser().Parse("e:1,f:-1.7,r:2,p:1,h:x7c,d:8,l:,");
  EXPECT_TRUE(exp.enabled);
  EXPECT_EQ(exp.factor, -1.7);
  EXPECT_EQ(exp.retries, 2);
  EXPECT_EQ(exp.ping, true);
  EXPECT_EQ(exp.duration.value().ms(), 8);
  EXPECT_FALSE(exp.latency);
}

TEST(StructParametersParserTest, UsesDefaults) {
  DummyConfig exp = DummyConfig::Parser().Parse("");
  EXPECT_FALSE(exp.enabled);
  EXPECT_EQ(exp.factor, 0.5);
  EXPECT_EQ(exp.retries, 5);
  EXPECT_EQ(exp.ping, false);
  EXPECT_EQ(exp.hash, "a80");
}

TEST(StructParametersParserTest, EmptyDefaults) {
  DummyConfig exp;
  auto encoded = DummyConfig::Parser().EncodeChanged(exp);
  // Unchanged parameters are not encoded.
  EXPECT_EQ(encoded, "");
}

TEST(StructParametersParserTest, EncodeAll) {
  DummyConfig exp;
  auto encoded = DummyConfig::Parser().EncodeAll(exp);
  // All parameters are encoded.
  EXPECT_EQ(encoded, "d:,e:false,f:0.5,h:a80,l:100 ms,p:false,r:5");
}

TEST(StructParametersParserTest, EncodeChanged) {
  DummyConfig exp;
  exp.ping = true;
  exp.retries = 4;
  auto encoded = DummyConfig::Parser().EncodeChanged(exp);
  // We expect the changed parameters to be encoded in alphabetical order.
  EXPECT_EQ(encoded, "p:true,r:4");
}

}  // namespace webrtc
