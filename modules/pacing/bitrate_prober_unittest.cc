/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/bitrate_prober.h"
#include "test/gtest.h"

namespace webrtc {

class BitrateProberTest : public ::testing::Test {
 protected:
  BitrateProberTest() : prober_(config_) { EXPECT_FALSE(prober_.IsProbing()); }
  const FieldTrialBasedConfig config_;
  BitrateProber prober_;
};

TEST_F(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober_.TimeUntilNextProbe(now_ms));

  const int kTestBitrate1 = 900000;
  const int kTestBitrate2 = 1800000;
  const int kClusterSize = 5;
  const int kProbeSize = 1000;
  const int kMinProbeDurationMs = 15;

  prober_.CreateProbeCluster(kTestBitrate1, now_ms, 0);
  prober_.CreateProbeCluster(kTestBitrate2, now_ms, 1);
  EXPECT_FALSE(prober_.IsProbing());

  prober_.OnIncomingPacket(kProbeSize);
  EXPECT_TRUE(prober_.IsProbing());
  EXPECT_EQ(0, prober_.CurrentCluster().probe_cluster_id);

  // First packet should probe as soon as possible.
  EXPECT_EQ(0, prober_.TimeUntilNextProbe(now_ms));

  for (int i = 0; i < kClusterSize; ++i) {
    now_ms += prober_.TimeUntilNextProbe(now_ms);
    EXPECT_EQ(0, prober_.TimeUntilNextProbe(now_ms));
    EXPECT_EQ(0, prober_.CurrentCluster().probe_cluster_id);
    prober_.ProbeSent(now_ms, kProbeSize);
  }

  EXPECT_GE(now_ms, kMinProbeDurationMs);
  // Verify that the actual bitrate is withing 10% of the target.
  double bitrate = kProbeSize * (kClusterSize - 1) * 8 * 1000.0 / now_ms;
  EXPECT_GT(bitrate, kTestBitrate1 * 0.9);
  EXPECT_LT(bitrate, kTestBitrate1 * 1.1);

  now_ms += prober_.TimeUntilNextProbe(now_ms);
  int64_t probe2_started = now_ms;

  for (int i = 0; i < kClusterSize; ++i) {
    now_ms += prober_.TimeUntilNextProbe(now_ms);
    EXPECT_EQ(0, prober_.TimeUntilNextProbe(now_ms));
    EXPECT_EQ(1, prober_.CurrentCluster().probe_cluster_id);
    prober_.ProbeSent(now_ms, kProbeSize);
  }

  // Verify that the actual bitrate is withing 10% of the target.
  int duration = now_ms - probe2_started;
  EXPECT_GE(duration, kMinProbeDurationMs);
  bitrate = kProbeSize * (kClusterSize - 1) * 8 * 1000.0 / duration;
  EXPECT_GT(bitrate, kTestBitrate2 * 0.9);
  EXPECT_LT(bitrate, kTestBitrate2 * 1.1);

  EXPECT_EQ(-1, prober_.TimeUntilNextProbe(now_ms));
  EXPECT_FALSE(prober_.IsProbing());
}

TEST_F(BitrateProberTest, DoesntProbeWithoutRecentPackets) {
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober_.TimeUntilNextProbe(now_ms));

  prober_.CreateProbeCluster(900000, now_ms, 0);
  EXPECT_FALSE(prober_.IsProbing());

  prober_.OnIncomingPacket(1000);
  EXPECT_TRUE(prober_.IsProbing());
  EXPECT_EQ(0, prober_.TimeUntilNextProbe(now_ms));
  prober_.ProbeSent(now_ms, 1000);
  // Let time pass, no large enough packets put into prober_.
  now_ms += 6000;
  EXPECT_EQ(-1, prober_.TimeUntilNextProbe(now_ms));
  // Check that legacy behaviour where prober is reset in TimeUntilNextProbe is
  // no longer there. Probes are no longer retried if they are timed out.
  prober_.OnIncomingPacket(1000);
  EXPECT_EQ(-1, prober_.TimeUntilNextProbe(now_ms));
}

TEST_F(BitrateProberTest, DoesntInitializeProbingForSmallPackets) {
  prober_.SetEnabled(true);
  EXPECT_FALSE(prober_.IsProbing());

  prober_.OnIncomingPacket(100);
  EXPECT_FALSE(prober_.IsProbing());
}

TEST_F(BitrateProberTest, VerifyProbeSizeOnHighBitrate) {
  constexpr unsigned kHighBitrateBps = 10000000;  // 10 Mbps

  prober_.CreateProbeCluster(kHighBitrateBps, 0, /*cluster_id=*/0);
  // Probe size should ensure a minimum of 1 ms interval.
  EXPECT_GT(prober_.RecommendedMinProbeSize(), kHighBitrateBps / 8000);
}

TEST_F(BitrateProberTest, MinumumNumberOfProbingPackets) {
  // Even when probing at a low bitrate we expect a minimum number
  // of packets to be sent.
  constexpr int kBitrateBps = 100000;  // 100 kbps
  constexpr int kPacketSizeBytes = 1000;

  prober_.CreateProbeCluster(kBitrateBps, 0, 0);
  prober_.OnIncomingPacket(kPacketSizeBytes);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(prober_.IsProbing());
    prober_.ProbeSent(0, kPacketSizeBytes);
  }

  EXPECT_FALSE(prober_.IsProbing());
}

TEST_F(BitrateProberTest, ScaleBytesUsedForProbing) {
  constexpr int kBitrateBps = 10000000;  // 10 Mbps
  constexpr int kPacketSizeBytes = 1000;
  constexpr int kExpectedBytesSent = kBitrateBps * 15 / 8000;

  prober_.CreateProbeCluster(kBitrateBps, 0, /*cluster_id=*/0);
  prober_.OnIncomingPacket(kPacketSizeBytes);
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober_.IsProbing());
    prober_.ProbeSent(0, kPacketSizeBytes);
    bytes_sent += kPacketSizeBytes;
  }

  EXPECT_FALSE(prober_.IsProbing());
}

TEST_F(BitrateProberTest, HighBitrateProbing) {
  constexpr int kBitrateBps = 1000000000;  // 1 Gbps.
  constexpr int kPacketSizeBytes = 1000;
  constexpr int kExpectedBytesSent = (kBitrateBps / 8000) * 15;

  prober_.CreateProbeCluster(kBitrateBps, 0, 0);
  prober_.OnIncomingPacket(kPacketSizeBytes);
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober_.IsProbing());
    prober_.ProbeSent(0, kPacketSizeBytes);
    bytes_sent += kPacketSizeBytes;
  }

  EXPECT_FALSE(prober_.IsProbing());
}

TEST_F(BitrateProberTest, ProbeClusterTimeout) {
  constexpr int kBitrateBps = 300000;  // 300 kbps
  constexpr int kSmallPacketSize = 20;
  // Expecting two probe clusters of 5 packets each.
  constexpr int kExpectedBytesSent = 20 * 2 * 5;
  constexpr int64_t kTimeoutMs = 5000;

  int64_t now_ms = 0;
  prober_.CreateProbeCluster(kBitrateBps, now_ms, /*cluster_id=*/0);
  prober_.OnIncomingPacket(kSmallPacketSize);
  EXPECT_FALSE(prober_.IsProbing());
  now_ms += kTimeoutMs;
  prober_.CreateProbeCluster(kBitrateBps / 10, now_ms, /*cluster_id=*/1);
  prober_.OnIncomingPacket(kSmallPacketSize);
  EXPECT_FALSE(prober_.IsProbing());
  now_ms += 1;
  prober_.CreateProbeCluster(kBitrateBps / 10, now_ms, /*cluster_id=*/2);
  prober_.OnIncomingPacket(kSmallPacketSize);
  EXPECT_TRUE(prober_.IsProbing());
  int bytes_sent = 0;
  while (bytes_sent < kExpectedBytesSent) {
    ASSERT_TRUE(prober_.IsProbing());
    prober_.ProbeSent(0, kSmallPacketSize);
    bytes_sent += kSmallPacketSize;
  }

  EXPECT_FALSE(prober_.IsProbing());
}
}  // namespace webrtc
