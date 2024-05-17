#include "audio/arrival_time_tracker.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "arrival_time_tracker.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ArrivalTimeTracker, ProvideCorrectTime) {
  ArrivalTimeTracker tracker(20);
  tracker.InsertPacket(/*rtp_timestamp=*/12345, /*sequence_number=*/3,
                       Timestamp::Seconds(10));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/12345),
            Timestamp::Seconds(10));
}

TEST(ArrivalTimeTracker, ProvideCorrectTimeWithOutOfOrderPackets) {
  ArrivalTimeTracker tracker(20);
  tracker.InsertPacket(/*rtp_timestamp=*/10, /*sequence_number=*/3,
                       Timestamp::Seconds(10));
  tracker.InsertPacket(/*rtp_timestamp=*/20, /*sequence_number=*/4,
                       Timestamp::Seconds(20));
  tracker.InsertPacket(/*rtp_timestamp=*/60, /*sequence_number=*/8,
                       Timestamp::Seconds(30));
  tracker.InsertPacket(/*rtp_timestamp=*/30, /*sequence_number=*/5,
                       Timestamp::Seconds(40));
  tracker.InsertPacket(/*rtp_timestamp=*/40, /*sequence_number=*/6,
                       Timestamp::Seconds(50));
  tracker.InsertPacket(/*rtp_timestamp=*/50, /*sequence_number=*/7,
                       Timestamp::Seconds(60));
  tracker.InsertPacket(/*rtp_timestamp=*/70, /*sequence_number=*/9,
                       Timestamp::Seconds(70));

  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/10),
            Timestamp::Seconds(10));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/22),
            Timestamp::Seconds(20));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/30),
            Timestamp::Seconds(40));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/31),
            Timestamp::Seconds(40));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/40),
            Timestamp::Seconds(50));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/62),
            Timestamp::Seconds(30));
  EXPECT_EQ(tracker.GetArrivalTime(/*rtp_timestamp=*/70),
            Timestamp::Seconds(70));
}

}  // namespace
}  // namespace webrtc