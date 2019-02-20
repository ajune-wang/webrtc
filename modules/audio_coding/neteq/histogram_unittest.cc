/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>

#include "modules/audio_coding/neteq/histogram.h"
#include "test/gtest.h"

namespace webrtc {

TEST(HistogramTest, Initialization) {
  Histogram histogram(65, 32440);
  histogram.Reset();
  const auto& buckets = histogram.buckets();
  double sum = 0.0;
  for (size_t i = 0; i < buckets.size(); i++) {
    EXPECT_NEAR(ldexp(std::pow(0.5, static_cast<int>(i + 1)), 30), buckets[i],
                65537);
    // Tolerance 65537 in Q30 corresponds to a delta of approximately 0.00006.
    sum += buckets[i];
  }
  EXPECT_EQ(1 << 30, static_cast<int>(sum));  // Should be 1 in Q30.
}

TEST(HistogramTest, Add) {
  Histogram histogram(10, 32440);
  histogram.Reset();
  const std::vector<int> before = histogram.buckets();
  const int index = 5;
  histogram.Add(index);
  const std::vector<int> after = histogram.buckets();
  EXPECT_GT(after[index], before[index]);
}

TEST(HistogramTest, ForgetFactor) {
  Histogram histogram(10, 32440);
  histogram.Reset();
  const std::vector<int> before = histogram.buckets();
  const int index = 4;
  histogram.Add(index);
  const std::vector<int> after = histogram.buckets();
  for (int i = 0; i < histogram.NumBuckets(); ++i) {
    if (i != index) {
      EXPECT_LT(after[i], before[i]);
    }
  }
}

// Test if the histogram is stretched correctly if the packet size is decreased.
TEST(HistogramScalingTest, StretchTest) {
  // Test a straightforward 60ms to 20ms change.
  std::vector<int> iat = {12, 0, 0, 0, 0, 0};
  std::vector<int> expected_result = {4, 4, 4, 0, 0, 0};
  std::vector<int> stretched_iat = Histogram::ScaleBuckets(iat, 60, 20);
  EXPECT_EQ(stretched_iat, expected_result);

  // Test an example where the last bin in the stretched histogram should
  // contain the sum of the elements that don't fit into the new histogram.
  iat = {18, 15, 12, 9, 6, 3, 0};
  expected_result = {6, 6, 6, 5, 5, 5, 30};
  stretched_iat = Histogram::ScaleBuckets(iat, 60, 20);
  EXPECT_EQ(stretched_iat, expected_result);

  // Test a 120ms to 60ms change.
  iat = {18, 16, 14, 4, 0};
  expected_result = {9, 9, 8, 8, 18};
  stretched_iat = Histogram::ScaleBuckets(iat, 120, 60);
  EXPECT_EQ(stretched_iat, expected_result);

  // Test a 120ms to 20ms change.
  iat = {19, 12, 0, 0, 0, 0, 0, 0};
  expected_result = {3, 3, 3, 3, 3, 3, 2, 11};
  stretched_iat = Histogram::ScaleBuckets(iat, 120, 20);
  EXPECT_EQ(stretched_iat, expected_result);

  // Test a 70ms to 40ms change.
  iat = {13, 7, 5, 3, 1, 5, 12, 11, 3, 0, 0, 0};
  expected_result = {7, 5, 5, 3, 3, 2, 2, 1, 2, 2, 6, 22};
  stretched_iat = Histogram::ScaleBuckets(iat, 70, 40);
  EXPECT_EQ(stretched_iat, expected_result);

  // Test a 30ms to 20ms change.
  iat = {13, 7, 5, 3, 1, 5, 12, 11, 3, 0, 0, 0};
  expected_result = {8, 6, 6, 3, 2, 2, 1, 3, 3, 8, 7, 11};
  stretched_iat = Histogram::ScaleBuckets(iat, 30, 20);
  EXPECT_EQ(stretched_iat, expected_result);
}

// Test if the histogram is compressed correctly if the packet size is
// increased.
TEST(HistogramScalingTest, CompressionTest) {
  // Test a 20 to 60 ms change.
  std::vector<int> iat = {12, 11, 10, 3, 2, 1};
  std::vector<int> expected_result = {33, 6, 0, 0, 0, 0};
  std::vector<int> compressed_iat = Histogram::ScaleBuckets(iat, 20, 60);
  EXPECT_EQ(compressed_iat, expected_result);

  // Test a 60ms to 120ms change.
  iat = {18, 16, 14, 4, 1};
  expected_result = {34, 18, 1, 0, 0};
  compressed_iat = Histogram::ScaleBuckets(iat, 60, 120);
  EXPECT_EQ(compressed_iat, expected_result);

  // Test a 20ms to 120ms change.
  iat = {18, 12, 5, 4, 4, 3, 5, 1};
  expected_result = {46, 6, 0, 0, 0, 0, 0, 0};
  compressed_iat = Histogram::ScaleBuckets(iat, 20, 120);
  EXPECT_EQ(compressed_iat, expected_result);

  // Test a 70ms to 80ms change.
  iat = {13, 7, 5, 3, 1, 5, 12, 11, 3};
  expected_result = {11, 8, 6, 2, 5, 12, 13, 3, 0};
  compressed_iat = Histogram::ScaleBuckets(iat, 70, 80);
  EXPECT_EQ(compressed_iat, expected_result);

  // Test a 50ms to 110ms change.
  iat = {13, 7, 5, 3, 1, 5, 12, 11, 3};
  expected_result = {18, 8, 16, 16, 2, 0, 0, 0, 0};
  compressed_iat = Histogram::ScaleBuckets(iat, 50, 110);
  EXPECT_EQ(compressed_iat, expected_result);
}

// Test if the histogram scaling function handles overflows correctly.
TEST(HistogramScalingTest, OverflowTest) {
  // Test a compression operation that can cause overflow.
  std::vector<int> iat = {733544448, 0, 0, 0, 0, 0, 0,
                          340197376, 0, 0, 0, 0, 0, 0};
  std::vector<int> expected_result = {733544448, 340197376, 0, 0, 0, 0, 0,
                                      0,         0,         0, 0, 0, 0, 0};
  std::vector<int> scaled_iat = Histogram::ScaleBuckets(iat, 10, 60);
  EXPECT_EQ(scaled_iat, expected_result);

  iat = {655591163, 39962288, 360736736, 1930514, 4003853, 1782764,
         114119,    2072996,  0,         2149354, 0};
  expected_result = {1056290187, 7717131, 2187115, 2149354, 0, 0,
                     0,          0,       0,       0,       0};
  scaled_iat = Histogram::ScaleBuckets(iat, 20, 60);
  EXPECT_EQ(scaled_iat, expected_result);

  // In this test case we will not be able to add everything to the final bin in
  // the scaled histogram. Check that the last bin doesn't overflow.
  iat = {2000000000, 2000000000, 2000000000,
         2000000000, 2000000000, 2000000000};
  expected_result = {666666666, 666666666, 666666666,
                     666666667, 666666667, 2147483647};
  scaled_iat = Histogram::ScaleBuckets(iat, 60, 20);
  EXPECT_EQ(scaled_iat, expected_result);

  // In this test case we will not be able to add enough to each of the bins,
  // so the values should be smeared out past the end of the normal range.
  iat = {2000000000, 2000000000, 2000000000,
         2000000000, 2000000000, 2000000000};
  expected_result = {2147483647, 2147483647, 2147483647,
                     2147483647, 2147483647, 1262581765};
  scaled_iat = Histogram::ScaleBuckets(iat, 20, 60);
  EXPECT_EQ(scaled_iat, expected_result);
}

}  // namespace webrtc
