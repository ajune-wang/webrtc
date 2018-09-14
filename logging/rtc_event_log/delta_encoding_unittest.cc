/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/delta_encoding.h"

#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

// TODO: !!!
  uint64_t TestMe(uint64_t previous,
                       uint64_t current,
                       uint64_t width);

namespace webrtc {
namespace {

// Encodes |values| based on |base|, then decodes the result and makes sure
// that it is equal to the original input.
// If |encoded_string| is non-null, the encoded result will also be written
// into it.
void TestEncodingAndDecoding(uint64_t base,
                             const std::vector<uint64_t>& values,
                             std::string* encoded_string = nullptr) {
  const std::string encoded = EncodeDeltas(base, values);
  if (encoded_string) {
    *encoded_string = encoded;
  }

  const std::vector<uint64_t> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

std::vector<uint64_t> CreateSequenceByFirstValue(uint64_t first,
                                                 size_t sequence_length) {
  std::vector<uint64_t> sequence(sequence_length);
  std::iota(sequence.begin(), sequence.end(), first);
  return sequence;
}

std::vector<uint64_t> CreateSequenceByLastValue(uint64_t last,
                                                size_t num_values) {
  const uint64_t first = last - num_values + 1;
  std::vector<uint64_t> result(num_values);
  std::iota(result.begin(), result.end(), first);
  return result;
}

// If |sequence_length| is greater than the number of deltas, the sequence of
// deltas will wrap around.
std::vector<uint64_t> CreateSequenceByDeltas(
    uint64_t first,
    const std::vector<uint64_t>& deltas,
    size_t sequence_length) {
  RTC_DCHECK_GE(sequence_length, 1);

  std::vector<uint64_t> sequence(sequence_length);

  uint64_t previous = first;
  for (size_t i = 0, next_delta_index = 0; i < sequence.size(); ++i) {
    sequence[i] = previous + deltas[next_delta_index];
    next_delta_index = (next_delta_index + 1) % deltas.size();
    previous = sequence[i];
  }

  return sequence;
}

// Tests of the delta encoding, parameterized by the number of values
// in the sequence created by the test.
class DeltaEncodingTest : public ::testing::TestWithParam<size_t> {
 public:
  ~DeltaEncodingTest() override = default;
};

TEST_P(DeltaEncodingTest, AllValuesEqualToBaseValue) {
  const uint64_t base = 3432;
  std::vector<uint64_t> values(GetParam());
  std::fill(values.begin(), values.end(), base);
  std::string encoded;
  TestEncodingAndDecoding(base, values, &encoded);

  // Additional requirement - the encoding should be efficient in this
  // case - the empty string will be used.
  EXPECT_TRUE(encoded.empty());
}

TEST_P(DeltaEncodingTest, MinDeltaNoWrapAround) {
  const uint64_t base = 3432;

  const auto values = CreateSequenceByFirstValue(base + 1, GetParam());
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, BigDeltaNoWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = 3432;

  const auto values = CreateSequenceByFirstValue(base + kBigDelta, GetParam());
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MaxDeltaNoWrapAround) {
  const uint64_t base = 3432;

  const auto values = CreateSequenceByLastValue(
      std::numeric_limits<uint64_t>::max(), GetParam());
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MinDeltaWithWrapAround) {
  const uint64_t base = std::numeric_limits<uint64_t>::max();

  const auto values = CreateSequenceByDeltas(0, {10, 3}, GetParam());
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

// Suppress "integral constant overflow" warning; this is the test's focus.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4307)
#endif
TEST_P(DeltaEncodingTest, BigDeltaWithWrapAround) {
  const uint64_t kBigDelta = 132828;
  const uint64_t base = std::numeric_limits<uint64_t>::max() - kBigDelta + 3;

  const auto values = CreateSequenceByFirstValue(base + kBigDelta, GetParam());
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

TEST_P(DeltaEncodingTest, MaxDeltaWithWrapAround) {
  const uint64_t base = 3432;

  const auto values = CreateSequenceByLastValue(3, GetParam());
  ASSERT_LT(values[values.size() - 1], base) << "Sanity; must wrap around";

  TestEncodingAndDecoding(base, values);
}

// If GetParam() == 1, a zero delta will yield an empty string; that's already
// covered by AllValuesEqualToBaseValue, but it doesn't hurt to test again.
// For all other cases, we have a new test.
TEST_P(DeltaEncodingTest, ZeroDelta) {
  const uint64_t base = 3432;

  // Arbitrary sequence of deltas with intentional zero deltas, as well as
  // consecutive zeros.
  const std::vector<uint64_t> deltas = {0,      312, 11, 1,  1, 0, 0, 12,
                                        400321, 3,   3,  12, 5, 0, 6};
  const auto values = CreateSequenceByDeltas(base, deltas, GetParam());

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_CASE_P(NumberOfValuesInSequence,
                        DeltaEncodingTest,
                        ::testing::Values(1, 2, 100, 10000));

// Similar to DeltaEncodingTest, but instead of semi-surgically producing
// specific cases, produce large amount of semi-realistic inputs.
class DeltaEncodingFuzzerLikeTest
    : public ::testing::TestWithParam<std::tuple<uint64_t, uint64_t>> {
 public:
  ~DeltaEncodingFuzzerLikeTest() override = default;
};

uint64_t RandomWithMaxBitWidth(Random* prng, uint64_t max_width) {
  RTC_DCHECK_GE(max_width, 1u);
  RTC_DCHECK_LE(max_width, 64u);

  const uint64_t low = prng->Rand(std::numeric_limits<uint32_t>::max());
  const uint64_t high =
      max_width > 32u ? prng->Rand(std::numeric_limits<uint32_t>::max()) : 0u;

  const uint64_t random_before_mask = (high << 32) | low;

  if (max_width < 64) {
    return random_before_mask & ((static_cast<uint64_t>(1) << max_width) - 1);
  } else {
    return random_before_mask;
  }
}

TEST_P(DeltaEncodingFuzzerLikeTest, Test) {
  const uint64_t delta_max_bit_width = std::get<0>(GetParam());
  const uint64_t num_of_values = std::get<1>(GetParam());
  const uint64_t base = 3432;

  Random prng(1983);
  std::vector<uint64_t> deltas(num_of_values);
  for (size_t i = 0; i < deltas.size(); i++) {
    deltas[i] = RandomWithMaxBitWidth(&prng, delta_max_bit_width);
  }

  const auto values = CreateSequenceByDeltas(base, deltas, num_of_values);

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_CASE_P(
    DeltaMaxBitWidthAndNumberOfValuesInSequence,
    DeltaEncodingFuzzerLikeTest,
    ::testing::Combine(::testing::Values(1, 4, 8, 32, 64),
                       ::testing::Values(1, 2, 100, 10000)));


// uint64_t BitWidth(uint64_t input) {
//   uint64_t width = 0;
//   do {  // input == 0 -> width == 1
//     width += 1;
//     input >>= 1;
//   } while (input != 0);
//   return width;
// }

// uint64_t MaxValueOfBitWidth(uint64_t bit_width) {
//   RTC_DCHECK_GE(bit_width, 1);
//   RTC_DCHECK_LE(bit_width, 64);
//   return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
//                            : ((static_cast<uint64_t>(1) << bit_width) - 1);
// }

TEST(ELAD, E1) {
  uint64_t x = 0xffffffffffffffff;
  uint64_t y = 0x7fffffffffffffff;
  // uint64_t x = 0xf;
  // uint64_t y = 0xf / 2;
  TestMe(x, y, 64);
}

}  // namespace
}  // namespace webrtc
