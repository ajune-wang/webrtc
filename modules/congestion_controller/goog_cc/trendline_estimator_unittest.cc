/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/trendline_estimator.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "api/transport/field_trial_based_config.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class PacketTimeGenerator {
 public:
  PacketTimeGenerator(int64_t initial_clock, double time_between_packets)
      : initial_clock_(initial_clock),
        time_between_packets_(time_between_packets),
        packets_(0) {}
  int64_t operator()() {
    return initial_clock_ + time_between_packets_ * packets_++;
  }

 private:
  const int64_t initial_clock_;
  const double time_between_packets_;
  size_t packets_;
};

class TrendlineEstimatorTest : public testing::Test {
 public:
  TrendlineEstimatorTest()
      : send_times(kPacketCount),
        recv_times(kPacketCount),
        packet_sizes(kPacketCount),
        config(),
        estimator(&config, nullptr),
        expected_detector_state() {
    std::fill(packet_sizes.begin(), packet_sizes.end(), kPacketSizeBytes);
  }
  void RunTest() {
    RTC_DCHECK_GE(expected_detector_state.size(), 1);
    RTC_DCHECK_EQ(send_times.size(), kPacketCount);
    RTC_DCHECK_EQ(recv_times.size(), kPacketCount);
    RTC_DCHECK_EQ(packet_sizes.size(), kPacketCount);

    size_t state_changes = 0;
    for (size_t i = 1; i < kPacketCount; i++) {
      double recv_delta = recv_times[i] - recv_times[i - 1];
      double send_delta = send_times[i] - send_times[i - 1];
      estimator.Update(recv_delta, send_delta, send_times[i], recv_times[i],
                       packet_sizes[i], true);
      BandwidthUsage state = estimator.State();
      if (state_changes + 1 < expected_detector_state.size() &&
          state == expected_detector_state[state_changes + 1]) {
        state_changes += 1;
      }
      EXPECT_EQ(state, expected_detector_state[state_changes]);
    }
    EXPECT_EQ(state_changes,
              expected_detector_state.size() -
                  1);  // The detector has gone through all states.
  }

 protected:
  const size_t kPacketCount = 25;
  const size_t kPacketSizeBytes = 1200;
  std::vector<int64_t> send_times;
  std::vector<int64_t> recv_times;
  std::vector<size_t> packet_sizes;
  const FieldTrialBasedConfig config;
  TrendlineEstimator estimator;
  std::vector<BandwidthUsage> expected_detector_state;
};
}  // namespace

TEST_F(TrendlineEstimatorTest, Normal) {
  PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
                                          20 /*20 ms between sent packets*/);
  std::generate(send_times.begin(), send_times.end(), send_time_generator);

  PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
                                          20 /*delivered at the same pace*/);
  std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

  expected_detector_state = {BandwidthUsage::kBwNormal};
  RunTest();
}

TEST_F(TrendlineEstimatorTest, Overusing) {
  PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
                                          20 /*20 ms between sent packets*/);
  std::generate(send_times.begin(), send_times.end(), send_time_generator);

  PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
                                          1.1 * 20 /*10% slower delivery*/);
  std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

  expected_detector_state = {BandwidthUsage::kBwNormal,
                             BandwidthUsage::kBwOverusing};
  RunTest();
}

TEST_F(TrendlineEstimatorTest, Underusing) {
  PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
                                          20 /*20 ms between sent packets*/);
  std::generate(send_times.begin(), send_times.end(), send_time_generator);

  PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
                                          0.85 * 20 /*15% faster delivery*/);
  std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

  expected_detector_state = {BandwidthUsage::kBwNormal,
                             BandwidthUsage::kBwUnderusing};
  RunTest();
}

TEST_F(TrendlineEstimatorTest, IncludesSmallPacketsByDefault) {
  PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
                                          20 /*20 ms between sent packets*/);
  std::generate(send_times.begin(), send_times.end(), send_time_generator);

  PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
                                          1.1 * 20 /*10% slower delivery*/);
  std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

  std::fill(packet_sizes.begin(), packet_sizes.end(), 100);
  expected_detector_state = {BandwidthUsage::kBwNormal,
                             BandwidthUsage::kBwOverusing};
  RunTest();
}

}  // namespace webrtc
