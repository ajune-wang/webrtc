/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_queue.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/timer/fake_timeout.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/mock_send_queue.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::MockFunction;
using State = ::dcsctp::RetransmissionQueue::State;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr uint32_t kArwnd = 100000;

class RetransmissionQueueTest : public testing::Test {
 protected:
  RetransmissionQueueTest()
      : gen_(MID(42)),
        timeout_manager_([this]() { return now_; }),
        timer_manager_([this]() { return timeout_manager_.CreateTimeout(); }),
        timer_(timer_manager_.CreateTimer("test/t3_rtx",
                                          []() { return absl::nullopt; })) {}

  std::function<SendQueue::DataToSend(int64_t, size_t)> CreateChunk() {
    return [this](int64_t now, size_t max_size) {
      return SendQueue::DataToSend{
          .data = gen_.Ordered({1, 2, 3, 4}, "BE"),
      };
    };
  }

  std::vector<TSN> GetSentPacketTSNs(RetransmissionQueue& buf) {
    std::vector<TSN> tsns;
    for (const auto& elem : buf.GetChunksToSend(now_, 10000)) {
      tsns.push_back(elem.first);
    }
    return tsns;
  }

  const DcSctpOptions options_;
  DataGenerator gen_;
  int64_t now_ = 0;
  FakeTimeoutManager timeout_manager_;
  TimerManager timer_manager_;
  NiceMock<MockFunction<void(int64_t rtt_ms)>> on_rtt_;
  NiceMock<MockFunction<void()>> on_outgoing_message_buffer_empty_;
  NiceMock<MockFunction<void()>> on_clear_retransmission_counter_;
  NiceMock<MockSendQueue> producer_;
  std::unique_ptr<Timer> timer_;
};

TEST_F(RetransmissionQueueTest, InitialAckedPrevTsn) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, SendOneChunk) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(buf), testing::ElementsAre(TSN(10)));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, SendOneChunkAndAck) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(buf), testing::ElementsAre(TSN(10)));

  buf.HandleAcknowledge(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(10), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, SendThreeChunksAndAckTwo) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(buf),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12)));

  buf.HandleAcknowledge(now_, SackChunk(TSN(11), kArwnd, {}, {}));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(11), State::kAcked),
                          std::make_pair(TSN(12), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, AckWithGapBlocksFromRFC4960Section334) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(buf),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  buf.HandleAcknowledge(now_, SackChunk(TSN(12), kArwnd,
                                        {SackChunk::GapAckBlock(2, 3),
                                         SackChunk::GapAckBlock(5, 5)},
                                        {}));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(12), State::kAcked),
                          std::make_pair(TSN(13), State::kNacked),
                          std::make_pair(TSN(14), State::kAcked),
                          std::make_pair(TSN(15), State::kAcked),
                          std::make_pair(TSN(16), State::kNacked),
                          std::make_pair(TSN(17), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, ResendPacketsWhenNackedThreeTimes) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(buf),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  // Send more chunks, but leave some as gaps to force retransmission after
  // three NACKs.

  // Send 18
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(buf), testing::ElementsAre(TSN(18)));

  // Ack 12, 14-15, 17-18
  buf.HandleAcknowledge(now_, SackChunk(TSN(12), kArwnd,
                                        {SackChunk::GapAckBlock(2, 3),
                                         SackChunk::GapAckBlock(5, 6)},
                                        {}));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(12), State::kAcked),
                          std::make_pair(TSN(13), State::kNacked),
                          std::make_pair(TSN(14), State::kAcked),
                          std::make_pair(TSN(15), State::kAcked),
                          std::make_pair(TSN(16), State::kNacked),
                          std::make_pair(TSN(17), State::kAcked),
                          std::make_pair(TSN(18), State::kAcked)));

  // Send 19
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(buf), testing::ElementsAre(TSN(19)));

  // Ack 12, 14-15, 17-19
  buf.HandleAcknowledge(now_, SackChunk(TSN(12), kArwnd,
                                        {SackChunk::GapAckBlock(2, 3),
                                         SackChunk::GapAckBlock(5, 7)},
                                        {}));

  // Send 20
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(buf), testing::ElementsAre(TSN(20)));

  // Ack 12, 14-15, 17-20
  buf.HandleAcknowledge(now_, SackChunk(TSN(12), kArwnd,
                                        {SackChunk::GapAckBlock(2, 3),
                                         SackChunk::GapAckBlock(5, 8)},
                                        {}));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(12), State::kAcked),
                          std::make_pair(TSN(13), State::kToBeRetransmitted),
                          std::make_pair(TSN(14), State::kAcked),
                          std::make_pair(TSN(15), State::kAcked),
                          std::make_pair(TSN(16), State::kToBeRetransmitted),
                          std::make_pair(TSN(17), State::kAcked),
                          std::make_pair(TSN(18), State::kAcked),
                          std::make_pair(TSN(19), State::kAcked),
                          std::make_pair(TSN(20), State::kAcked)));

  // This will trigger "fast retransmit" mode and only chunks 13 and 16 will be
  // resent right now. The send queue will not even be queried.
  EXPECT_CALL(producer_, Produce).Times(0);

  EXPECT_THAT(GetSentPacketTSNs(buf), testing::ElementsAre(TSN(13), TSN(16)));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(12), State::kAcked),
                          std::make_pair(TSN(13), State::kInFlight),
                          std::make_pair(TSN(14), State::kAcked),
                          std::make_pair(TSN(15), State::kAcked),
                          std::make_pair(TSN(16), State::kInFlight),
                          std::make_pair(TSN(17), State::kAcked),
                          std::make_pair(TSN(18), State::kAcked),
                          std::make_pair(TSN(19), State::kAcked),
                          std::make_pair(TSN(20), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, CanOnlyProduceTwoPacketsButWantsToSendThree) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "BE"),
        };
      })
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{.data = gen_.Ordered({1, 2, 3, 4}, "BE")};
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _)));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight),
                          std::make_pair(TSN(11), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, RetransmitsOnT3Expiry) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "BE"),
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));

  // Will force chunks to be retransmitted
  buf.HandleT3RtxTimerExpiry();

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kToBeRetransmitted)));

  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kToBeRetransmitted)));

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_rtx, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, LimitedRetransmissionOnlyWithRfc3758Support) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_,
      /*supports_partial_reliability=*/false);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "BE"),
            .max_retransmissions = 0,
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));

  // Will force chunks to be retransmitted
  buf.HandleT3RtxTimerExpiry();

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kToBeRetransmitted)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(0);
  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
}

TEST_F(RetransmissionQueueTest, LimitsRetransmissionsAsUdp) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "BE"),
            .max_retransmissions = 0,
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));

  // Will force chunks to be retransmitted
  buf.HandleT3RtxTimerExpiry();

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kToBeRetransmitted)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);

  EXPECT_TRUE(buf.ShouldSendForwardTsn(now_));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kAbandoned)));

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_rtx, testing::IsEmpty());
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kAbandoned)));
}

TEST_F(RetransmissionQueueTest, LimitsRetransmissionsToThreeSends) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "BE"),
            .max_retransmissions = 3,
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(0);

  // Retransmission 1
  buf.HandleT3RtxTimerExpiry();
  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  EXPECT_THAT(buf.GetChunksToSend(now_, 1000), SizeIs(1));

  // Retransmission 2
  buf.HandleT3RtxTimerExpiry();
  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  EXPECT_THAT(buf.GetChunksToSend(now_, 1000), SizeIs(1));

  // Retransmission 3
  buf.HandleT3RtxTimerExpiry();
  EXPECT_FALSE(buf.ShouldSendForwardTsn(now_));
  EXPECT_THAT(buf.GetChunksToSend(now_, 1000), SizeIs(1));

  // Retransmission 4 - not allowed.
  buf.HandleT3RtxTimerExpiry();
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  EXPECT_TRUE(buf.ShouldSendForwardTsn(now_));
  EXPECT_THAT(buf.GetChunksToSend(now_, 1000), IsEmpty());

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kAbandoned)));
}

TEST_F(RetransmissionQueueTest, RetransmitsWhenSendBufferIsFullT3Expiry) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  static constexpr size_t kCwnd = 1200;
  buf.set_cwnd(kCwnd);
  EXPECT_EQ(buf.cwnd(), kCwnd);
  EXPECT_EQ(buf.outstanding_bytes(), 0u);

  std::vector<uint8_t> payload(1000);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this, payload](int64_t, size_t) {
        return SendQueue::DataToSend{.data = gen_.Ordered(payload, "BE")};
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1500);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));
  EXPECT_EQ(buf.outstanding_bytes(), payload.size() + DataChunk::kHeaderSize);

  // Will force chunks to be retransmitted
  buf.HandleT3RtxTimerExpiry();

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kToBeRetransmitted)));
  EXPECT_EQ(buf.outstanding_bytes(), 0u);

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      buf.GetChunksToSend(now_, 1500);
  EXPECT_THAT(chunks_to_rtx, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight)));
  EXPECT_EQ(buf.outstanding_bytes(), payload.size() + DataChunk::kHeaderSize);
}

TEST_F(RetransmissionQueueTest, ProducesValidForwardTsn) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "B"),
            .max_retransmissions = 0,
        };
      })
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({5, 6, 7, 8}, ""),
            .max_retransmissions = 0,
        };
      })
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({9, 10, 11, 12}, ""),
            .max_retransmissions = 0,
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  // Send and ack first chunk (TSN 10)
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight),
                          std::make_pair(TSN(11), State::kInFlight),
                          std::make_pair(TSN(12), State::kInFlight)));

  // Chunk 10 is acked, but the remaining are lost
  buf.HandleAcknowledge(now_, SackChunk(TSN(10), kArwnd, {}, {}));
  buf.HandleT3RtxTimerExpiry();

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(10), State::kAcked),
                          std::make_pair(TSN(11), State::kToBeRetransmitted),
                          std::make_pair(TSN(12), State::kToBeRetransmitted)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  EXPECT_TRUE(buf.ShouldSendForwardTsn(now_));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(10), State::kAcked),
                          std::make_pair(TSN(11), State::kAbandoned),
                          std::make_pair(TSN(12), State::kAbandoned)));

  ForwardTsnChunk forward_tsn = buf.CreateForwardTsn();
  EXPECT_EQ(forward_tsn.new_cumulative_tsn(), TSN(12));
  EXPECT_THAT(forward_tsn.skipped_streams(),
              UnorderedElementsAre(
                  ForwardTsnChunk::SkippedStream(StreamID(1), SSN(42))));
}

TEST_F(RetransmissionQueueTest, ProducesValidIForwardTsn) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_,
      /*supports_partial_reliability=*/true, /*use_message_interleaving=*/true);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "B", {.stream_id = StreamID(1)}),
            .max_retransmissions = 0,
        };
      })
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data =
                gen_.Unordered({1, 2, 3, 4}, "B", {.stream_id = StreamID(2)}),
            .max_retransmissions = 0,
        };
      })
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data =
                gen_.Ordered({9, 10, 11, 12}, "B", {.stream_id = StreamID(3)}),
            .max_retransmissions = 0,
        };
      })
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data =
                gen_.Ordered({13, 14, 15, 16}, "B", {.stream_id = StreamID(4)}),
            .max_retransmissions = 0,
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _), Pair(TSN(13), _)));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kInFlight),
                          std::make_pair(TSN(11), State::kInFlight),
                          std::make_pair(TSN(12), State::kInFlight),
                          std::make_pair(TSN(13), State::kInFlight)));

  // Chunk 13 is acked, but the remaining are lost
  buf.HandleAcknowledge(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(4, 4)}, {}));
  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kNacked),
                          std::make_pair(TSN(11), State::kNacked),
                          std::make_pair(TSN(12), State::kNacked),
                          std::make_pair(TSN(13), State::kAcked)));

  buf.HandleT3RtxTimerExpiry();

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kToBeRetransmitted),
                          std::make_pair(TSN(11), State::kToBeRetransmitted),
                          std::make_pair(TSN(12), State::kToBeRetransmitted),
                          std::make_pair(TSN(13), State::kAcked)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  EXPECT_CALL(producer_, Discard(IsUnordered(true), StreamID(2), MID(42)))
      .Times(1);
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(3), MID(42)))
      .Times(1);
  EXPECT_TRUE(buf.ShouldSendForwardTsn(now_));

  EXPECT_THAT(buf.GetChunkStates(),
              ElementsAre(std::make_pair(TSN(9), State::kAcked),
                          std::make_pair(TSN(10), State::kAbandoned),
                          std::make_pair(TSN(11), State::kAbandoned),
                          std::make_pair(TSN(12), State::kAbandoned),
                          std::make_pair(TSN(13), State::kAcked)));

  IForwardTsnChunk forward_tsn = buf.CreateIForwardTsn();
  EXPECT_EQ(forward_tsn.new_cumulative_tsn(), TSN(12));
  EXPECT_THAT(
      forward_tsn.skipped_streams(),
      UnorderedElementsAre(IForwardTsnChunk::SkippedStream(
                               IsUnordered(false), StreamID(1), MID(42)),
                           IForwardTsnChunk::SkippedStream(
                               IsUnordered(true), StreamID(2), MID(42)),
                           IForwardTsnChunk::SkippedStream(
                               IsUnordered(false), StreamID(3), MID(42))));
}

TEST_F(RetransmissionQueueTest, MeasureRTT) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_,
      /*supports_partial_reliability=*/true, /*use_message_interleaving=*/true);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](int64_t, size_t) {
        return SendQueue::DataToSend{
            .data = gen_.Ordered({1, 2, 3, 4}, "B"),
            .max_retransmissions = 0,
        };
      })
      .WillRepeatedly([](int64_t, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      buf.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));

  now_ += 123;

  EXPECT_CALL(on_rtt_, Call(123)).Times(1);
  buf.HandleAcknowledge(now_, SackChunk(TSN(10), kArwnd, {}, {}));
}

TEST_F(RetransmissionQueueTest, OldSacksAreNotUsed) {
  RetransmissionQueue buf(
      "", TSN(10), kArwnd, &producer_, on_rtt_.AsStdFunction(),
      on_outgoing_message_buffer_empty_.AsStdFunction(),
      on_clear_retransmission_counter_.AsStdFunction(), timer_.get(), options_,
      /*supports_partial_reliability=*/true, /*use_message_interleaving=*/true);

  buf.HandleAcknowledge(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_FALSE(buf.IsAcknowledgeValid(SackChunk(TSN(9), kArwnd, {}, {})));
  EXPECT_TRUE(buf.IsAcknowledgeValid(SackChunk(TSN(10), kArwnd, {}, {})));
  EXPECT_TRUE(buf.IsAcknowledgeValid(SackChunk(TSN(11), kArwnd, {}, {})));
}
}  // namespace
}  // namespace dcsctp
