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

#include <algorithm>
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
    return VerifyAssociations(associations.begin(), associations.end());
  }

  void VerifyAssociations(
      std::vector<Association>::const_iterator associations_begin,
      std::vector<Association>::const_iterator associations_end) {
    RTC_DCHECK(associations_begin < associations_end);
    ASSERT_EQ(static_cast<size_t>(associations_end - associations_begin),
              AssociationCount());
    for (auto association = associations_begin; association != associations_end;
         ++association) {
      const auto info = uut_->Get(association->sequence_number);
      ASSERT_TRUE(info);
      EXPECT_EQ(info, association->info);
    }
  }

  size_t AssociationCount() const { return uut_->AssociationCountForTesting(); }

  // Allows several variations of the same test; definition next to the tests.
  void GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      bool with_wrap_around,
      bool last_element_kept);

  // Allows several variations of the same test; definition next to the tests.
  void RepeatedSequenceNumberInvalidatesAll(size_t index_of_repeated);

  // Allows several variations of the same test; definition next to the tests.
  void MaxEntriesReachedAtSameTimeAsObsoletionOfItem(size_t max_entries,
                                                     size_t obsoleted_count);

  Random random_;
  std::unique_ptr<RtpSequenceNumberMap> uut_;  // Unit under test.
};

class RtpSequenceNumberMapTestWithParams
    : public RtpSequenceNumberMapTest,
      public ::testing::WithParamInterface<
          std::tuple<size_t, uint16_t, bool, bool>> {
 protected:
  RtpSequenceNumberMapTestWithParams()
      : association_count_(std::get<0>(GetParam())),
        first_sequence_number_(std::get<1>(GetParam())),
        first_info_(0, std::get<2>(GetParam()), std::get<3>(GetParam())) {}

  ~RtpSequenceNumberMapTestWithParams() override = default;

  const size_t association_count_;
  const uint16_t first_sequence_number_;
  const Info first_info_;
};

INSTANTIATE_TEST_SUITE_P(_,
                         RtpSequenceNumberMapTestWithParams,
                         ::testing::Combine(
                             // Associations
                             ::testing::Values(1, 2, 100),
                             // First sequence number.
                             ::testing::Values(0,
                                               100,
                                               uint16_t_max - 100,
                                               uint16_t_max - 1,
                                               uint16_t_max),
                             // Is first packet in frame.
                             ::testing::Bool(),
                             // Is last packet in frame.
                             ::testing::Bool()));

TEST_F(RtpSequenceNumberMapTest, GetBeforeAssociationsRecordedReturnsNullOpt) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);
  constexpr uint16_t kArbitrarySequenceNumber = 321;
  EXPECT_FALSE(uut_->Get(kArbitrarySequenceNumber));
}

// Version #1 - any old unknown sequence number.
TEST_F(RtpSequenceNumberMapTest, GetUnknownSequenceNumberReturnsNullOpt1) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  constexpr uint16_t kKnownSequenceNumber = 10;
  constexpr uint16_t kArbitrary = 987;
  Insert(kKnownSequenceNumber, kArbitrary, false, false);

  constexpr uint16_t kUnknownSequenceNumber = kKnownSequenceNumber + 1;
  EXPECT_FALSE(Get(kUnknownSequenceNumber));
}

// Version #2 - intentionally pick a value in the range of currently held
// values, so as to trigger lower_bound / upper_bound.
TEST_F(RtpSequenceNumberMapTest, GetUnknownSequenceNumberReturnsNullOpt2) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);
  const std::vector<Association> setup = {CreateAssociation(1000, 500),
                                          CreateAssociation(1020, 501)};
  for (const Association& association : setup) {
    Insert(association);
  }
  EXPECT_FALSE(Get(1001));
}

TEST_P(RtpSequenceNumberMapTestWithParams,
       GetKnownSequenceNumberReturnsCorrectValue) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  uint16_t sequence_number = first_sequence_number_;
  Info info = first_info_;
  std::vector<Association> associations;
  associations.reserve(association_count_);
  for (size_t i = 0; i < association_count_; ++i) {
    // This test may not include old entry obsoletion.
    // (Unlike sequence numbers, values *may* be repeated.)
    RTC_DCHECK(i == 0 ||
               AheadOf(sequence_number, associations[0].sequence_number));

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

void RtpSequenceNumberMapTest::
    GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
        bool with_wrap_around,
        bool last_element_kept) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

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

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted0) {
  const bool with_wrap_around = false;
  const bool last_element_kept = false;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted1) {
  const bool with_wrap_around = true;
  const bool last_element_kept = false;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted2) {
  const bool with_wrap_around = false;
  const bool last_element_kept = true;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

TEST_F(RtpSequenceNumberMapTest,
       GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted3) {
  const bool with_wrap_around = true;
  const bool last_element_kept = true;
  GetObsoleteSequenceNumberReturnsNullOptMultipleEntriesObsoleted(
      with_wrap_around, last_element_kept);
}

void RtpSequenceNumberMapTest::RepeatedSequenceNumberInvalidatesAll(
    size_t index_of_repeated) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  const std::vector<Association> setup = {CreateAssociation(100, 500),
                                          CreateAssociation(101, 501),
                                          CreateAssociation(102, 502)};
  RTC_DCHECK_LT(index_of_repeated, setup.size());
  for (const Association& association : setup) {
    Insert(association);
  }

  const Association new_association =
      CreateAssociation(setup[index_of_repeated].sequence_number, 503);
  Insert(new_association);

  // All entries from setup invalidated.
  // New entry valid and mapped to new value.
  for (size_t i = 0; i < setup.size(); ++i) {
    if (i == index_of_repeated) {
      ASSERT_TRUE(Get(new_association.sequence_number));
      EXPECT_EQ(*Get(new_association.sequence_number), new_association.info);
    } else {
      EXPECT_FALSE(Get(setup[i].sequence_number));
    }
  }
}

TEST_F(RtpSequenceNumberMapTest,
       RepeatedSequenceNumberInvalidatesAllRepeatFirst) {
  RepeatedSequenceNumberInvalidatesAll(0);
}

TEST_F(RtpSequenceNumberMapTest,
       RepeatedSequenceNumberInvalidatesAllRepeatMiddle) {
  RepeatedSequenceNumberInvalidatesAll(1);
}

TEST_F(RtpSequenceNumberMapTest,
       RepeatedSequenceNumberInvalidatesAllRepeatLast) {
  RepeatedSequenceNumberInvalidatesAll(2);
}

TEST_F(RtpSequenceNumberMapTest,
       SequenceNumberInsideMemorizedRangeInvalidatesAll) {
  CreateUnitUnderTest(kMaxPossibleMaxEntries);

  const std::vector<Association> setup = {CreateAssociation(1000, 500),
                                          CreateAssociation(1020, 501),
                                          CreateAssociation(1030, 502)};
  for (const Association& association : setup) {
    Insert(association);
  }

  const Association new_association = CreateAssociation(1010, 503);
  Insert(new_association);

  // All entries from setup invalidated.
  // New entry valid and mapped to new value.
  for (size_t i = 0; i < setup.size(); ++i) {
    EXPECT_FALSE(Get(setup[i].sequence_number));
  }
  ASSERT_TRUE(Get(new_association.sequence_number));
  EXPECT_EQ(*Get(new_association.sequence_number), new_association.info);
}

TEST_F(RtpSequenceNumberMapTest, MaxEntriesObserved) {
  constexpr size_t kMaxEntries = 100;
  CreateUnitUnderTest(kMaxEntries);

  std::vector<Association> associations;
  associations.reserve(kMaxEntries);
  uint32_t timestamp = 789;
  for (size_t i = 0; i < kMaxEntries; ++i) {
    associations.push_back(CreateAssociation(i, ++timestamp));
    Insert(associations[i]);
  }
  VerifyAssociations(associations);  // Sanity.

  const Association new_association =
      CreateAssociation(kMaxEntries, ++timestamp);
  Insert(new_association);
  associations.push_back(new_association);

  // The +1 is for |new_association|.
  const size_t kExpectedAssociationCount = 3 * kMaxEntries / 4 + 1;
  const auto expected_begin =
      std::prev(associations.end(), kExpectedAssociationCount);
  VerifyAssociations(expected_begin, associations.end());
}

void RtpSequenceNumberMapTest::MaxEntriesReachedAtSameTimeAsObsoletionOfItem(
    size_t max_entries,
    size_t obsoleted_count) {
  CreateUnitUnderTest(max_entries);

  std::vector<Association> associations;
  associations.reserve(max_entries);
  uint32_t timestamp = 789;
  for (size_t i = 0; i < max_entries; ++i) {
    associations.push_back(CreateAssociation(i, ++timestamp));
    Insert(associations[i]);
  }
  VerifyAssociations(associations);  // Sanity.

  const uint16_t new_association_sequence_number =
      static_cast<uint16_t>(obsoleted_count) + (1 << 15);
  const Association new_association =
      CreateAssociation(new_association_sequence_number, ++timestamp);
  Insert(new_association);
  associations.push_back(new_association);

  // The +1 is for |new_association|.
  const size_t kExpectedAssociationCount =
      std::min(3 * max_entries / 4, max_entries - obsoleted_count) + 1;
  const auto expected_begin =
      std::prev(associations.end(), kExpectedAssociationCount);
  VerifyAssociations(expected_begin, associations.end());
}

// Version #1 - #(obsoleted entries) < #(entries after paring down below max).
TEST_F(RtpSequenceNumberMapTest,
       MaxEntriesReachedAtSameTimeAsObsoletionOfItem1) {
  constexpr size_t kMaxEntries = 100;
  constexpr size_t kObsoletionTarget = (kMaxEntries / 4) - 1;
  MaxEntriesReachedAtSameTimeAsObsoletionOfItem(kMaxEntries, kObsoletionTarget);
}

// Version #2 - #(obsoleted entries) == #(entries after paring down below max).
TEST_F(RtpSequenceNumberMapTest,
       MaxEntriesReachedAtSameTimeAsObsoletionOfItem2) {
  constexpr size_t kMaxEntries = 100;
  constexpr size_t kObsoletionTarget = kMaxEntries / 4;
  MaxEntriesReachedAtSameTimeAsObsoletionOfItem(kMaxEntries, kObsoletionTarget);
}

// Version #3 - #(obsoleted entries) > #(entries after paring down below max).
TEST_F(RtpSequenceNumberMapTest,
       MaxEntriesReachedAtSameTimeAsObsoletionOfItem3) {
  constexpr size_t kMaxEntries = 100;
  constexpr size_t kObsoletionTarget = (kMaxEntries / 4) + 1;
  MaxEntriesReachedAtSameTimeAsObsoletionOfItem(kMaxEntries, kObsoletionTarget);
}

}  // namespace
}  // namespace webrtc
