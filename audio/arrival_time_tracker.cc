#include "audio/arrival_time_tracker.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace webrtc {

ArrivalTimeTracker::ArrivalTimeTracker(size_t size)
    : buffer_(size, std::nullopt) {}

void ArrivalTimeTracker::Reset() {
  std::fill(buffer_.begin(), buffer_.end(), std::nullopt);
  read_index_ = 0;
  last_written_index_ = 0;
}

void ArrivalTimeTracker::InsertPacket(uint32_t rtp_timestamp,
                                      uint16_t sequence_number,
                                      webrtc::Timestamp arrival_time) {
  if (!start_sequence_number_) {
    start_sequence_number_ = sequence_number;
  }
  size_t next_write_index =
      (sequence_number - *start_sequence_number_) % buffer_.size();
  if (read_index_ > last_written_index_ && read_index_ < next_write_index) {
    // The circular buffer will overrun.
    Reset();
    next_write_index = 0;
    start_sequence_number_ = sequence_number;
  }

  buffer_[next_write_index] = {.rtp_timestamp = rtp_timestamp,
                               .arrival_time = arrival_time};

  last_written_index_ = next_write_index;
}

std::optional<webrtc::Timestamp> ArrivalTimeTracker::GetArrivalTime(
    uint32_t rtp_timestamp) {
  size_t i = read_index_;
  if (read_index_ > last_written_index_) {
    for (; i <= buffer_.size(); ++i) {
      if (!buffer_[i].has_value())
        continue;
      if (buffer_[i]->rtp_timestamp <= rtp_timestamp) {
        size_t previous_index = i == 0 ? buffer_.size() - 1 : i - 1;
        buffer_[previous_index].reset();
        read_index_ = i;
      } else {
        break;
      }
    }
    i = 0;
  }
  for (; i <= last_written_index_; ++i) {
    if (!buffer_[i].has_value())
      continue;
    if (buffer_[i]->rtp_timestamp <= rtp_timestamp) {
      size_t previous_index = i == 0 ? buffer_.size() - 1 : i - 1;
      buffer_[previous_index].reset();
      read_index_ = i;
    } else {
      break;
    }
  }

  if (buffer_[read_index_]) {
    return buffer_[read_index_]->arrival_time;
  }
  return std::nullopt;
}

}  // namespace webrtc