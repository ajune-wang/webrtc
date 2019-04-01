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
  Association(uint16_t sequence_number, Info info)
      : sequence_number(sequence_number), info(info) {}

  uint16_t sequence_number;
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

  Association CreateAssociation(uint16_t sequence_number, uint32_t timestamp) {
    return Association(sequence_number,
                       {timestamp, random_.Rand<bool>(), random_.Rand<bool>()});
  }

  void Insert(uint16_t sequence_number,
              uint32_t timestamp,
              bool is_first,
              bool is_last) {
    Insert(sequence_number, {timestamp, is_first, is_last});
  }

  void Insert(uint16_t sequence_number, Info info) {
    uut_->Insert(sequence_number, info);
  }

  void Insert(const Association& association) {
    Insert(association.sequence_number, association.info);
  }

  absl::optional<Info> Get(uint16_t sequence_number) {
    return uut_->Get(sequence_number);
  }

  void VerifyAssociations(const std::vector<Association>& associations) {
    ASSERT_EQ(associations.size(), uut_->AssociationCountForTesting());
    for (auto association : associations) {
      const auto info = uut_->Get(association.sequence_number);
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
  constexpr uint16_t kArbitrarySequenceNumber = 321;
  EXPECT_FALSE(uut_->Get(kArbitrarySequenceNumber));
}

TEST_F(RtpSequenceNumberMapTest, GetUnknownSequenceNumberReturnsNullOpt) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  constexpr uint16_t kKnownSequenceNumber = 10;
  constexpr uint16_t kArbitrary = 987;
  Insert(kKnownSequenceNumber, kArbitrary, false, false);

  constexpr uint16_t kUnknownSequenceNumber = kKnownSequenceNumber + 1;
  EXPECT_FALSE(Get(kUnknownSequenceNumber));
}

TEST_P(RtpSequenceNumberMapTestWithParams,
       GetKnownSequenceNumberReturnsCorrectValue) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  const size_t association_count = Param<0, size_t>();
  const uint16_t first_sequence_number = Param<1, uint16_t>();
  const Info first_info = {Param<2, uint32_t>(), Param<3, bool>(),
                           Param<4, bool>()};

  uint16_t sequence_number = first_sequence_number;
  Info info = first_info;
  std::vector<Association> associations;
  associations.reserve(association_count);
  for (size_t i = 0; i < association_count; ++i) {
    // This test may not include old entry obsoletion.
    // (Unlike sequence numbers, values *may* be repeated.)
    RTC_DCHECK(i == 0 ||
               AheadOf(sequence_number, associations[0].sequence_number));
    RTC_DCHECK(i == 0 ||
               AheadOf(info.timestamp, associations[0].info.timestamp));

    // Record.
    Insert(sequence_number, info);

    // Memorize.
    associations.emplace_back(sequence_number, info);

    // Produce the next iteration's values.
    sequence_number += (1 + random_.Rand(99));
    info = {static_cast<uint32_t>(info.timestamp + 1 + random_.Rand(9999)),
            random_.Rand<bool>(), random_.Rand<bool>()};
  }

  for (auto association : associations) {
    absl::optional<Info> info = Get(association.sequence_number);
    ASSERT_TRUE(info);
    EXPECT_EQ(*info, association.info);
  }
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptSingleValueObsoleted) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  const std::vector<Association> associations = {
      CreateAssociation(0, 10), CreateAssociation(0x8000u, 20),
      CreateAssociation(0x8001u, 30)};

  Insert(associations[0]);

  // First association not yet obsolete, and therefore remembered.
  RTC_DCHECK(AheadOf(associations[1].sequence_number,
                     associations[0].sequence_number));
  Insert(associations[1]);
  VerifyAssociations({associations[0], associations[1]});

  // Test focus - new entry obsoletes first entry.
  RTC_DCHECK(!AheadOf(associations[2].sequence_number,
                      associations[0].sequence_number));
  Insert(associations[2]);
  VerifyAssociations({associations[1], associations[2]});
}

TEST_P(RtpSequenceNumberMapTestWithBoolParams,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted) {
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
      static_cast<uint16_t>(
          associations[associations.size() - 1].sequence_number),
      static_cast<uint16_t>(
          associations[associations.size() - 2].sequence_number + 1u));
  uint16_t new_sequence_number;
  if (last_element_kept) {
    new_sequence_number =
        associations[associations.size() - 1].sequence_number + 0x8000u;
    RTC_DCHECK(AheadOf(new_sequence_number,
                       associations[associations.size() - 1].sequence_number));
  } else {
    new_sequence_number =
        associations[associations.size() - 1].sequence_number + 0x8001u;
    RTC_DCHECK(!AheadOf(new_sequence_number,
                        associations[associations.size() - 1].sequence_number));
  }
  RTC_DCHECK(!AheadOf(new_sequence_number,
                      associations[associations.size() - 2].sequence_number));

  // Record the new association.
  const Association new_association =
      CreateAssociation(new_sequence_number, 60);
  Insert(new_association);

  // Make sure all obsoleted elements were removed.
  const size_t obsoleted_count =
      associations.size() - (last_element_kept ? 1 : 0);
  for (size_t i = 0; i < obsoleted_count; ++i) {
    EXPECT_FALSE(Get(associations[i].sequence_number));
  }

  // Make sure the expected elements were not removed, and return the
  // expected value.
  if (last_element_kept) {
    EXPECT_TRUE(Get(associations.back().sequence_number));
    EXPECT_EQ(Get(associations.back().sequence_number),
              associations.back().info);
  }
  EXPECT_TRUE(Get(new_association.sequence_number));
  EXPECT_EQ(Get(new_association.sequence_number), new_association.info);
}

}  // namespace
}  // namespace webrtc
