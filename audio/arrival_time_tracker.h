#ifndef ARRIVAL_TIME_TRACKER_H_
#define ARRIVAL_TIME_TRACKER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/units/timestamp.h"

namespace webrtc {

class ArrivalTimeTracker {
 public:
  ArrivalTimeTracker(size_t size);

  void InsertPacket(uint32_t rtp_timestamp,
                    uint16_t sequence_number,
                    webrtc::Timestamp arrival_time);

  // The requested rtp_timestamp have to be in increasing order.
  std::optional<webrtc::Timestamp> GetArrivalTime(uint32_t rtp_timestamp);

  void Reset();

 private:
  struct Entry {
    uint32_t rtp_timestamp;
    webrtc::Timestamp arrival_time;
  };
  std::vector<std::optional<Entry>> buffer_;
  std::optional<uint16_t> start_sequence_number_;
  size_t read_index_ = 0;
  size_t last_written_index_ = 0;
};

}  // namespace webrtc

#endif  // ARRIVAL_TIME_TRACKER_H_