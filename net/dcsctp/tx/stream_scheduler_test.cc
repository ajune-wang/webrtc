/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/stream_scheduler.h"

#include "net/dcsctp/public/types.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::Return;
using ::testing::StrictMock;

MATCHER_P(HasDataWithMid, mid, "") {
  if (!arg.has_value()) {
    *result_listener << "There was no produced data";
    return false;
  }

  if (arg->data.message_id != mid) {
    *result_listener << "the produced data had mid " << *arg->data.message_id
                     << " and not the expected " << *mid;
    return false;
  }

  return true;
}

std::function<absl::optional<SendQueue::DataToSend>(TimeMs, size_t)>
CreateChunk(StreamID sid, MID mid) {
  return [sid, mid](TimeMs now, size_t max_size) {
    return SendQueue::DataToSend(Data(sid, SSN(0), mid, FSN(0), PPID(42),
                                      {1, 2, 3, 4}, Data::IsBeginning(true),
                                      Data::IsEnd(true), IsUnordered(true)));
  };
}

class MockStreamCallback : public StreamScheduler::StreamCallback {
 public:
  MOCK_METHOD(absl::optional<SendQueue::DataToSend>,
              Produce,
              (TimeMs, size_t),
              (override));
  MOCK_METHOD(size_t, bytes_to_send_in_next_message, (), (const, override));
};

TEST(StreamSchedulerTest, HasNoActiveStreams) {
  StreamScheduler scheduler;

  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
}

TEST(StreamSchedulerTest, CanProduceFromSingleStream) {
  StrictMock<MockStreamCallback> callback;
  EXPECT_CALL(callback, Produce).WillOnce(CreateChunk(StreamID(1), MID(0)));
  EXPECT_CALL(callback, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(0));

  StreamScheduler scheduler;
  auto stream =
      scheduler.CreateStream(&callback, StreamID(1), StreamPriority(2));
  stream->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(0)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
}

TEST(StreamSchedulerTest, WillRoundRobinBetweenStreams) {
  StrictMock<MockStreamCallback> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(0)))
      .WillOnce(CreateChunk(StreamID(1), MID(2)))
      .WillOnce(CreateChunk(StreamID(1), MID(4)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(4))
      .WillOnce(Return(0));

  StreamScheduler scheduler;
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamCallback> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(StreamID(2), MID(1)))
      .WillOnce(CreateChunk(StreamID(2), MID(3)))
      .WillOnce(CreateChunk(StreamID(2), MID(5)));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(4))
      .WillOnce(Return(0));

  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(0)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(1)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(2)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(3)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(4)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(5)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
}

TEST(StreamSchedulerTest, StreamsCanBeMadeInactive) {
  StrictMock<MockStreamCallback> callback1;

  // Callbacks are setup so that they hint that there is a MID(2) coming...
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(0)))
      .WillOnce(CreateChunk(StreamID(1), MID(1)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(4));

  StreamScheduler scheduler;
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(0)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(1)));

  // ... but the stream is made inactive before it can be produced.
  stream1->MakeInactive();
  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
}

TEST(StreamSchedulerTest, SingleStreamCanBeResumed) {
  StrictMock<MockStreamCallback> callback1;

  // Callbacks are setup so that they hint that there is a MID(2) coming...
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(0)))
      .WillOnce(CreateChunk(StreamID(1), MID(1)))
      .WillOnce(CreateChunk(StreamID(1), MID(2)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(4))
      .WillOnce(Return(4))  // When making active again
      .WillOnce(Return(0));

  StreamScheduler scheduler;
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(0)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(1)));

  stream1->MakeInactive();
  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
  stream1->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(2)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
}

TEST(StreamSchedulerTest, WillRoundRobinWithPausedStream) {
  StrictMock<MockStreamCallback> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(0)))
      .WillOnce(CreateChunk(StreamID(1), MID(2)))
      .WillOnce(CreateChunk(StreamID(1), MID(4)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(0));

  StreamScheduler scheduler;
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamCallback> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(StreamID(2), MID(1)))
      .WillOnce(CreateChunk(StreamID(2), MID(3)))
      .WillOnce(CreateChunk(StreamID(2), MID(5)));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(4))  // When making active
      .WillOnce(Return(4))
      .WillOnce(Return(4))
      .WillOnce(Return(0));

  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(0)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(1)));
  stream1->MakeInactive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(3)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(5)));
  stream1->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(2)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), 1000), HasDataWithMid(MID(4)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), 1000), absl::nullopt);
}

}  // namespace
}  // namespace dcsctp
