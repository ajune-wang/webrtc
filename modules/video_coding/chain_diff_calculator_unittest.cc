/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/chain_diff_calculator.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::SizeIs;

TEST(ChainDiffCalculatorTest, SingleChain) {
  // Simulate  a stream with 2 temporal layer where chain
  // protects temporal layer 0.
  ChainDiffCalculator calculator;
  // Key frame.
  calculator.Reset(1, std::bitset<32>(0b1));
  EXPECT_THAT(calculator.From(1, 0b1), ElementsAre(0));
  // T1 delta frame.
  EXPECT_THAT(calculator.From(2, 0b0), ElementsAre(1));
  // T0 delta frame.
  EXPECT_THAT(calculator.From(3, 0b1), ElementsAre(2));
}

TEST(ChainDiffCalculatorTest, TwoChainsFullSvc) {
  // Simulate a full svc stream with 2 spatial and 2 temporal layers.
  // chains are protecting temporal layers 0.
  ChainDiffCalculator calculator;
  // S0 Key frame.
  calculator.Reset(2, 0b11);
  EXPECT_THAT(calculator.From(1, 0b11), ElementsAre(0, 0));
  // S1 Key frame.
  EXPECT_THAT(calculator.From(2, 0b10), ElementsAre(1, 1));
  // S0T1 delta frame.
  EXPECT_THAT(calculator.From(3, 0b00), ElementsAre(2, 1));
  // S1T1 delta frame.
  EXPECT_THAT(calculator.From(4, 0b00), ElementsAre(3, 2));
  // S0T0 delta frame.
  EXPECT_THAT(calculator.From(5, 0b11), ElementsAre(4, 3));
  // S1T0 delta frame.
  EXPECT_THAT(calculator.From(6, 0b10), ElementsAre(1, 1));
}

TEST(ChainDiffCalculatorTest, TwoChainsKSvc) {
  // Simulate a k-svc stream with 2 spatial and 2 temporal layers.
  // chains are protecting temporal layers 0.
  ChainDiffCalculator calculator;
  // S0 Key frame.
  calculator.Reset(2, 0b11);
  EXPECT_THAT(calculator.From(1, 0b11), ElementsAre(0, 0));
  // S1 Key frame.
  EXPECT_THAT(calculator.From(2, 0b10), ElementsAre(1, 1));
  // S0T1 delta frame.
  EXPECT_THAT(calculator.From(3, 0b00), ElementsAre(2, 1));
  // S1T1 delta frame.
  EXPECT_THAT(calculator.From(4, 0b00), ElementsAre(3, 2));
  // S0T0 delta frame.
  EXPECT_THAT(calculator.From(5, 0b11), ElementsAre(4, 3));
  // S1T0 delta frame.
  EXPECT_THAT(calculator.From(6, 0b10), ElementsAre(1, 4));
}

TEST(ChainDiffCalculatorTest, TwoChainsSimulcast) {
  // Simulate a k-svc stream with 2 spatial and 2 temporal layers.
  // chains are protecting temporal layers 0.
  ChainDiffCalculator calculator;
  // S0 Key frame.
  calculator.Reset(2, 0b01);
  EXPECT_THAT(calculator.From(1, 0b01), ElementsAre(0, 0));
  // S1 Key frame.
  calculator.Reset(2, 0b10);
  EXPECT_THAT(calculator.From(2, 0b10), ElementsAre(1, 0));
  // S0T1 delta frame.
  EXPECT_THAT(calculator.From(3, 0b00), ElementsAre(2, 1));
  // S1T1 delta frame.
  EXPECT_THAT(calculator.From(4, 0b00), ElementsAre(3, 2));
  // S0T0 delta frame.
  EXPECT_THAT(calculator.From(5, 0b01), ElementsAre(4, 3));
  // S1T0 delta frame.
  EXPECT_THAT(calculator.From(6, 0b10), ElementsAre(1, 4));
}

TEST(ChainDiffCalculatorTest, ResilentToAbsentChainConfig) {
  ChainDiffCalculator calculator;
  // Key frame.
  calculator.Reset(2, 0b01);
  EXPECT_THAT(calculator.From(1, 0b01), ElementsAre(0, 0));
  // Forgot to set chains. should still return 2 chain_diffs.
  EXPECT_THAT(calculator.From(2, 0), ElementsAre(1, 0));
  // chain diffs for next frame(s) are undefined, but still there should be
  // correct number of them.
  EXPECT_THAT(calculator.From(3, 0b01), SizeIs(2));
  EXPECT_THAT(calculator.From(4, 0b10), SizeIs(2));
  // Since previous two frames updated all the chains, can expect what
  // chain_diffs would be.
  EXPECT_THAT(calculator.From(5, 0b00), ElementsAre(2, 1));
}

TEST(ChainDiffCalculatorTest, ResilentToTooMainChains) {
  ChainDiffCalculator calculator;
  // Key frame.
  calculator.Reset(2, 0b01);
  EXPECT_THAT(calculator.From(1, 0b01), ElementsAre(0, 0));
  // Set wrong number of chains. Expect number of chain_diffs is not changed.
  EXPECT_THAT(calculator.From(2, 0b111), ElementsAre(1, 0));
  // chain diffs for next frame(s) are undefined, but still there should be
  // correct number of them.
  EXPECT_THAT(calculator.From(3, 0b01), SizeIs(2));
  EXPECT_THAT(calculator.From(4, 0b10), SizeIs(2));
  // Since previous two frames updated all the chains, can expect what
  // chain_diffs would be.
  EXPECT_THAT(calculator.From(5, 0b00), ElementsAre(2, 1));
}

}  // namespace
}  // namespace webrtc
