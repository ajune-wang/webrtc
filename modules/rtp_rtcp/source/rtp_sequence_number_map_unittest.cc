/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"

#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
using Info = RtpSequenceNumberMap::Info;

namespace {
constexpr uint16_t uint16_t_max = std::numeric_limits<uint16_t>::max();
constexpr uint32_t uint32_t_max = std::numeric_limits<uint32_t>::max();
constexpr size_t kMaxPossibleMaxEntries = (1 << 15) + 1;

// Just a named pair.
struct Association final {
  // TODO: !!! s/key/sequence_number
  Association(uint16_t key, Info info) : key(key), info(info) {}

  uint16_t key;
  Info info;
};

class RtpSequenceNumberMapTest : public ::testing::Test {
 protected:
  static constexpr uint64_t kSeed = 1983;

  RtpSequenceNumberMapTest() : random_(kSeed) {}
  ~RtpSequenceNumberMapTest() override = default;

  void CreateUnitUnderTest(size_t max_entries) {
    uut_ = absl::make_unique<RtpSequenceNumberMap>(max_entries);
  }

  Association CreateAssociation(uint16_t key, uint32_t timestamp) {
    return Association(key,
                       {timestamp, random_.Rand<bool>(), random_.Rand<bool>()});
  }

  void Insert(uint16_t key,
              uint32_t val1timestamp,
              bool is_first,
              bool is_last) {
    Insert(key, {val1timestamp, is_first, is_last});
  }

  void Insert(uint16_t key, Info info) { uut_->Insert(key, info); }

  void Insert(const Association& association) {
    Insert(association.key, association.info);
  }

  absl::optional<Info> Get(uint16_t key) { return uut_->Get(key); }

  void VerifyAssociations(const std::vector<Association>& associations) {
    for (auto association : associations) {
      const auto info = uut_->Get(association.key);
      ASSERT_TRUE(info);
      EXPECT_EQ(info, association.info);
    }
  }

  Random random_;
  std::unique_ptr<RtpSequenceNumberMap> uut_;  // Unit under test.
};

class RtpSequenceNumberMapTestWithParams
    : public RtpSequenceNumberMapTest,
      public ::testing::WithParamInterface<
          std::tuple<size_t, uint16_t, uint32_t, bool, bool>> {
 protected:
  RtpSequenceNumberMapTestWithParams() = default;
  ~RtpSequenceNumberMapTestWithParams() override = default;

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
    RtpSequenceNumberMapTestWithParams,
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

class RtpSequenceNumberMapTestWithBoolParams
    : public RtpSequenceNumberMapTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  RtpSequenceNumberMapTestWithBoolParams() = default;
  ~RtpSequenceNumberMapTestWithBoolParams() override = default;

  // Arbitrary parameterized values, to be used by the tests whenever they
  // wish to either check some combinations, or wish to demonstrate that
  // a particular arbitrary value is unimportant.
  template <size_t N>
  bool Param() const {
    return std::get<N>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(_,
                         RtpSequenceNumberMapTestWithBoolParams,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(RtpSequenceNumberMapTest, GetBeforeAssociationsRecordedReturnsNullOpt) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);
  constexpr uint16_t kArbitraryKey = 321;
  EXPECT_FALSE(uut_->Get(kArbitraryKey));
}

TEST_F(RtpSequenceNumberMapTest, GetUnknownKeyReturnsNullOpt) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  constexpr uint16_t kKnownKey = 10;
  constexpr uint16_t kArbitrary = 987;
  Insert(kKnownKey, kArbitrary, false, false);

  constexpr uint16_t kUnknownKey = kKnownKey + 1;
  EXPECT_FALSE(Get(kUnknownKey));
}

TEST_P(RtpSequenceNumberMapTestWithParams, GetKnownKeyReturnsCorrectValue) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  const size_t association_count = Param<0, size_t>();
  const uint16_t first_key = Param<1, uint16_t>();
  const Info first_info = {Param<2, uint32_t>(), Param<3, bool>(),
                           Param<4, bool>()};

  uint16_t key = first_key;
  Info info = first_info;
  std::vector<Association> associations;
  associations.reserve(association_count);
  for (size_t i = 0; i < association_count; ++i) {
    // This test may not include old entry obsoletion.
    // (Unlike keys, values *may* be repeated.)
    RTC_DCHECK(i == 0 || AheadOf(key, associations[0].key));
    RTC_DCHECK(i == 0 ||
               AheadOf(info.timestamp, associations[0].info.timestamp));

    // Record.
    Insert(key, info);

    // Memorize.
    associations.emplace_back(key, info);

    // Produce the next iteration's values.
    key = static_cast<uint16_t>(key + 1 + random_.Rand(99));
    info = {static_cast<uint32_t>(info.timestamp + 1 + random_.Rand(9999)),
            random_.Rand<bool>(), random_.Rand<bool>()};
  }

  for (auto association : associations) {
    absl::optional<Info> info = Get(association.key);
    ASSERT_TRUE(info);
    EXPECT_EQ(*info, association.info);
  }
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteKeyReturnsNullOptSingleValueObsoleted) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  const std::vector<Association> associations = {
      CreateAssociation(0, 10), CreateAssociation(0x8000u, 20),
      CreateAssociation(0x8001u, 30)};

  Insert(associations[0]);

  // First association not yet obsolete, and therefore remembered.
  RTC_DCHECK(AheadOf(associations[1].key, associations[0].key));
  Insert(associations[1]);
  VerifyAssociations({associations[0], associations[1]});

  // Test focus - new entry obsoletes first entry.
  RTC_DCHECK(!AheadOf(associations[2].key, associations[0].key));
  Insert(associations[2]);
  VerifyAssociations({associations[1], associations[2]});
}

TEST_P(RtpSequenceNumberMapTestWithBoolParams,
       GetObsoleteKeyReturnsNullOptMultipleEntriesObsoleted) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

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
    Insert(association);
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
  Insert(new_association);

  // Make sure all obsoleted elements were removed.
  const size_t obsoleted_count =
      associations.size() - (last_element_kept ? 1 : 0);
  for (size_t i = 0; i < obsoleted_count; ++i) {
    EXPECT_FALSE(Get(associations[i].key));
  }

  // Make sure the expected elements were not removed, and return the
  // expected value.
  if (last_element_kept) {
    EXPECT_TRUE(Get(associations.back().key));
    EXPECT_EQ(Get(associations.back().key), associations.back().info);
  }
  EXPECT_TRUE(Get(new_association.key));
  EXPECT_EQ(Get(new_association.key), new_association.info);
}

}  // namespace
}  // namespace webrtc
