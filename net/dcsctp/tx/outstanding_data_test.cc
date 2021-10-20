/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/outstanding_data.h"

#include "absl/types/optional.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
namespace dcsctp {
namespace {
using ::testing::MockFunction;
using State = ::dcsctp::OutstandingData::State;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr TimeMs kNow(42);

class OutstandingDataTest : public testing::Test {
 protected:
  OutstandingDataTest()
      : gen_(MID(42)),
        buf_(DataChunk::kHeaderSize,
             unwrapper_.Unwrap(TSN(10)),
             unwrapper_.Unwrap(TSN(9)),
             on_discard_.AsStdFunction()) {}

  UnwrappedTSN::Unwrapper unwrapper_;
  DataGenerator gen_;
  testing::NiceMock<testing::MockFunction<bool(IsUnordered, StreamID, MID)>>
      on_discard_;
  OutstandingData buf_;
};

TEST_F(OutstandingDataTest, HasInitialState) {
  EXPECT_TRUE(buf_.empty());
  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(10));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(9));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked)));
  EXPECT_FALSE(buf_.ShouldSendForwardTsn());
}

TEST_F(OutstandingDataTest, InsertChunk) {
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UnwrappedTSN tsn,
      buf_.Insert(gen_.Ordered({1}, "BE"), absl::nullopt, kNow, absl::nullopt));

  EXPECT_EQ(tsn.Wrap(), TSN(10));

  EXPECT_EQ(buf_.outstanding_bytes(), DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.outstanding_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksSingleChunk) {
  buf_.Insert(gen_.Ordered({1}, "BE"), absl::nullopt, kNow, absl::nullopt);
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(10)), {}, false);

  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(10));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(10));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked)));
}

TEST_F(OutstandingDataTest, AcksPreviousChunkDoesntUpdate) {
  buf_.Insert(gen_.Ordered({1}, "BE"), absl::nullopt, kNow, absl::nullopt);
  buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), {}, false);

  EXPECT_EQ(buf_.outstanding_bytes(), DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.outstanding_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksAndNacksWithGapAckBlocks) {
  buf_.Insert(gen_.Ordered({1}, "B"), absl::nullopt, kNow, absl::nullopt);
  buf_.Insert(gen_.Ordered({1}, "E"), absl::nullopt, kNow, absl::nullopt);

  std::vector<SackChunk::GapAckBlock> gab = {SackChunk::GapAckBlock(2, 2)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(11));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(12));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(11));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesWithSameTsnDoesntRetransmit) {
  buf_.Insert(gen_.Ordered({1}, "B"), absl::nullopt, kNow, absl::nullopt);
  buf_.Insert(gen_.Ordered({1}, "E"), absl::nullopt, kNow, absl::nullopt);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInRetransmission) {
  buf_.Insert(gen_.Ordered({1}, "B"), absl::nullopt, kNow, absl::nullopt);
  buf_.Insert(gen_.Ordered({1}, ""), absl::nullopt, kNow, absl::nullopt);
  buf_.Insert(gen_.Ordered({1}, ""), absl::nullopt, kNow, absl::nullopt);
  buf_.Insert(gen_.Ordered({1}, "E"), absl::nullopt, kNow, absl::nullopt);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),               //
                          Pair(TSN(10), State::kToBeRetransmitted),  //
                          Pair(TSN(11), State::kAcked),              //
                          Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kAcked)));

  EXPECT_THAT(buf_.GetChunksToBeRetransmitted(1000),
              ElementsAre(Pair(TSN(10), _)));
}
}  // namespace
}  // namespace dcsctp
