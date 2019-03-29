/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rename_me.h"

#include <iterator>
#include <limits>
#include <tuple>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
using Value = RecoveryRequestAdapter::Value;

namespace {
constexpr uint16_t uint16_t_max = std::numeric_limits<uint16_t>::max();
constexpr uint32_t uint32_t_max = std::numeric_limits<uint32_t>::max();

// Just a named pair.
struct Association {
  Association() = default;
  Association(uint16_t key, Value value) : key(key), value(value) {}

  uint16_t key;
  Value value;
};

class RecoveryRequestAdapterTest : public ::testing::Test {
 protected:
  static constexpr uint64_t kSeed = 1983;

  RecoveryRequestAdapterTest() : random_(kSeed) {}
  ~RecoveryRequestAdapterTest() override = default;

  Association CreateAssociation(uint16_t key, uint32_t value_rtp_timestamp) {
    return Association(
        key, {value_rtp_timestamp, random_.Rand<bool>(), random_.Rand<bool>()});
  }

  void RecordNewAssociation(uint16_t key, uint32_t val1, bool val2, bool val3) {
    RecordNewAssociation(key, {val1, val2, val3});
  }

  void RecordNewAssociation(uint16_t key, Value value) {
    rra_.RecordNewAssociation(key, value);
  }

  void RecordNewAssociation(const Association& association) {
    RecordNewAssociation(association.key, association.value);
  }

  absl::optional<Value> GetValue(uint16_t key) { return rra_.GetValue(key); }

  void VerifyAssociations(const std::vector<Association>& associations) {
    for (auto association : associations) {
      const auto value = rra_.GetValue(association.key);
      ASSERT_TRUE(value);
      EXPECT_EQ(value, association.value);
    }
  }

  Random random_;
  RecoveryRequestAdapter rra_;
};

class RecoveryRequestAdapterTestWithParams
    : public RecoveryRequestAdapterTest,
      public ::testing::WithParamInterface<
          std::tuple<size_t, uint16_t, uint32_t, bool, bool>> {
 protected:
  RecoveryRequestAdapterTestWithParams() = default;
  ~RecoveryRequestAdapterTestWithParams() override = default;

  // Arbitrary parameterized values, to be used by the tests whenever they
  // wish to either check some combinations, or wish to demonstrate that
  // a particular arbitrary value is unimportant.
  // TODO(eladalon): Infer T.
  template <size_t N, typename T>
  T Param() const {
    return std::get<N>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    _,
    RecoveryRequestAdapterTestWithParams,
    ::testing::Combine(::testing::Values(1, 2, 100),
                       ::testing::Values(0,
                                         100,
                                         uint16_t_max - 100,
                                         uint16_t_max - 1,
                                         uint16_t_max),
                       ::testing::Values(0,
                                         100,
                                         uint32_t_max - 100,
                                         uint32_t_max - 1,
                                         uint32_t_max),
                       ::testing::Bool(),
                       ::testing::Bool()));

class RecoveryRequestAdapterTestWithBoolParams
    : public RecoveryRequestAdapterTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  RecoveryRequestAdapterTestWithBoolParams() = default;
  ~RecoveryRequestAdapterTestWithBoolParams() override = default;

  // Arbitrary parameterized values, to be used by the tests whenever they
  // wish to either check some combinations, or wish to demonstrate that
  // a particular arbitrary value is unimportant.
  template <size_t N>
  bool Param() const {
    return std::get<N>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(_,
                         RecoveryRequestAdapterTestWithBoolParams,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(RecoveryRequestAdapterTest,
       GetValueBeforeAssociationsRecordedReturnsNullOpt) {
  constexpr uint16_t kArbitraryKey = 321;
  EXPECT_FALSE(rra_.GetValue(kArbitraryKey));
}

TEST_F(RecoveryRequestAdapterTest, GetValueOnUnknownKeyReturnsNullOpt) {
  constexpr uint16_t kKnownKey = 10;
  constexpr uint16_t kArbitraryValue = 987;
  RecordNewAssociation(kKnownKey, kArbitraryValue, false, false);

  constexpr uint16_t kUnknownKey = kKnownKey + 1;
  EXPECT_FALSE(GetValue(kUnknownKey));
}

TEST_P(RecoveryRequestAdapterTestWithParams,
       GetValueOnKnownKeyReturnsCorrectValue) {
  const size_t association_count = Param<0, size_t>();
  const uint16_t first_key = Param<1, uint16_t>();
  const Value first_value = {Param<2, uint32_t>(), Param<3, bool>(),
                             Param<4, bool>()};

  uint16_t key = first_key;
  Value value = first_value;
  std::vector<Association> associations(association_count);
  for (size_t i = 0; i < association_count; ++i) {
    // This test may not include old entry obsoletion.
    // (Unlike keys, values *may* be repeated.)
    RTC_DCHECK(i == 0 || AheadOf(key, associations[0].key));
    RTC_DCHECK(i == 0 || AheadOf(value.rtp_timestamp,
                                 associations[0].value.rtp_timestamp));

    // Record.
    RecordNewAssociation(key, value);

    // Memorize.
    associations[i] = Association(key, value);

    // Produce the next iteration's values.
    key = static_cast<uint16_t>(key + 1 + random_.Rand(99));
    value = {
        static_cast<uint32_t>(value.rtp_timestamp + 1 + random_.Rand(9999)),
        random_.Rand<bool>(), random_.Rand<bool>()};
  }

  for (auto association : associations) {
    absl::optional<Value> value = GetValue(association.key);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, association.value);
  }
}

TEST_F(RecoveryRequestAdapterTest,
       GetValueOnObsoleteKeyReturnsNullOptSingleValueObsoleted) {
  const std::vector<Association> associations = {
      CreateAssociation(0, 10), CreateAssociation(0x8000u, 20),
      CreateAssociation(0x8001u, 30)};

  RecordNewAssociation(associations[0]);

  // First association not yet obsolete, and therefore remembered.
  RTC_DCHECK(AheadOf(associations[1].key, associations[0].key));
  RecordNewAssociation(associations[1]);
  VerifyAssociations({associations[0], associations[1]});

  // Test focus - new entry obsoletes first entry.
  RTC_DCHECK(!AheadOf(associations[2].key, associations[0].key));
  RecordNewAssociation(associations[2]);
  VerifyAssociations({associations[1], associations[2]});
}

TEST_P(RecoveryRequestAdapterTestWithBoolParams,
       GetValueOnObsoleteKeyReturnsNullOptMultipleEntriesObsoleted) {
  const bool with_wrap_around = Param<0>();
  const bool last_element_kept = Param<1>();

  std::vector<Association> associations;
  if (with_wrap_around) {
    associations = {CreateAssociation(uint16_t_max - 1, 10),
                    CreateAssociation(uint16_t_max, 20),
                    CreateAssociation(0, 30), CreateAssociation(1, 40),
                    CreateAssociation(2, 50)};
  } else {
    associations = {CreateAssociation(1, 10), CreateAssociation(2, 20),
                    CreateAssociation(3, 30), CreateAssociation(4, 40),
                    CreateAssociation(5, 50)};
  }

  // Start with all of the associations which
  for (auto association : associations) {
    RecordNewAssociation(association);
  }
  VerifyAssociations(associations);

  // Define a new association that will obsolete either all previous entries,
  // or all previous entries except for the last one, depending on the
  // parameter instantiation of this test.
  RTC_DCHECK_EQ(
      static_cast<uint16_t>(associations[associations.size() - 1].key),
      static_cast<uint16_t>(associations[associations.size() - 2].key + 1u));
  uint16_t new_key;
  if (last_element_kept) {
    new_key = associations[associations.size() - 1].key + 0x8000u;
    RTC_DCHECK(AheadOf(new_key, associations[associations.size() - 1].key));
  } else {
    new_key = associations[associations.size() - 1].key + 0x8001u;
    RTC_DCHECK(!AheadOf(new_key, associations[associations.size() - 1].key));
  }
  RTC_DCHECK(!AheadOf(new_key, associations[associations.size() - 2].key));

  // Record the new association.
  const Association new_association = CreateAssociation(new_key, 60);
  RecordNewAssociation(new_association);

  // Make sure all obsoleted elements were removed.
  const size_t obsoleted_count =
      associations.size() - (last_element_kept ? 1 : 0);
  for (size_t i = 0; i < obsoleted_count; ++i) {
    EXPECT_FALSE(GetValue(associations[i].key));
  }

  // Make sure the expected elements were not removed, and return the
  // expected value.
  if (last_element_kept) {
    EXPECT_TRUE(GetValue(associations.back().key));
    EXPECT_EQ(GetValue(associations.back().key), associations.back().value);
  }
  EXPECT_TRUE(GetValue(new_association.key));
  EXPECT_EQ(GetValue(new_association.key), new_association.value);
}

// TODO(eladalon): Add the following unit tests:
// 1. Repeated values (not keys).
// 2. Wrap-around of values (not keys).
// 3. Robustness against repeated (not just wrapped-around) keys. This can
//    happen if a sender sleeps for a long time, then wakes up with exactly
//    the same RTP timestamp as it had last used.
// TODO(eladalon): In light of #3 above (sender sleeps for a long time), we
// need to handle, on a different level, such cases, because if !AheadOf,
// this unit would miss that it is now holding stale - and probably by
// now incorrect - associations.

}  // namespace
}  // namespace webrtc
