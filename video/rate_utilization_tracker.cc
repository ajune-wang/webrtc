/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rate_utilization_tracker.h"

#include <algorithm>

namespace webrtc {

RateUtilizationTracker::RateUtilizationTracker(
    size_t max_num_encoded_data_points,
    TimeDelta max_duration)
    : max_data_points_(max_num_encoded_data_points),
      max_duration_(max_duration) {
  RTC_CHECK_GE(max_num_encoded_data_points, 0);
  RTC_CHECK_GT(max_duration, TimeDelta::Zero());
}

void RateUtilizationTracker::OnDataRateChanged(DataRate rate, Timestamp time) {
  RTC_CHECK(rate_updates_.empty() || time >= rate_updates_.back().time);
  rate_updates_.push_back({.rate = rate, .time = time});

  // Cull old updates - but make sure we always keep at least one. Only one
  // instance is allowed to be older than the duration limit.
  while (rate_updates_.size() > 1 &&
         time - rate_updates_[1].time > max_duration_) {
    rate_updates_.pop_front();
  }
}

void RateUtilizationTracker::OnDataProduced(DataSize size, Timestamp time) {
  RTC_DCHECK(data_points_.empty() || time >= data_points_.back().time);
  if (!data_points_.empty() && data_points_.back().time >= time) {
    data_points_.back().size += size;
  } else {
    data_points_.push_back({.size = size, .time = time});
  }

  // Cull old updates.
  while (data_points_.size() > max_data_points_ ||
         (!data_points_.empty() &&
          time - data_points_.front().time > max_duration_)) {
    data_points_.pop_front();
  }
}

absl::optional<double> RateUtilizationTracker::GetRateUtilizationFactor(
    Timestamp time) const {
  if (data_points_.empty() || rate_updates_.empty()) {
    // No data points or cannot calculate utilization due to lack of rate
    // updates.
    return absl::nullopt;
  }

  RTC_CHECK_GE(time, rate_updates_.back().time);
  RTC_CHECK_GE(time, data_points_.back().time);

  const Timestamp start_time = time.ms() < max_duration_.ms()
                                   ? Timestamp::Millis(0)
                                   : time - max_duration_;
  Timestamp time_of_first_included_data_point = Timestamp::MinusInfinity();
  DataSize total_produced_data = DataSize::Zero();
  for (const DataPoint& data_point : data_points_) {
    if (data_point.time >= start_time) {
      if (time_of_first_included_data_point.IsInfinite()) {
        time_of_first_included_data_point = data_point.time;
      }
      total_produced_data += data_point.size;
    }
  }
  const DataPoint& last_data_point = data_points_.back();

  DataSize data_allocated_for_last_data_point = DataSize::Zero();
  DataSize allocated_send_data_size = DataSize::Zero();

  for (size_t i = 0; i < rate_updates_.size(); ++i) {
    const DataRateUpdate& update = rate_updates_[i];
    const bool is_last_update = i >= rate_updates_.size() - 1;
    if (is_last_update) {
      data_allocated_for_last_data_point +=
          (time - std::max(last_data_point.time, update.time)) * update.rate;
    } else {
      // There is at least one more data point in the list after the current.

      const DataRateUpdate& next_update = rate_updates_[i + 1];
      if (next_update.time <= time_of_first_included_data_point) {
        // This rate update is older than the oldest valid rate update, ignore.
        continue;
      }

      if (update.time < last_data_point.time &&
          next_update.time >= last_data_point.time) {
        // The last data point is between this rate update and the next. Mark
        // time interval past the data point as counted towards the
        // tail-allocation.
        data_allocated_for_last_data_point +=
            (next_update.time - last_data_point.time) * update.rate;
      } else if (update.time >= last_data_point.time) {
        data_allocated_for_last_data_point +=
            (next_update.time - update.time) * update.rate;
      }
    }

    Timestamp rate_update_start =
        std::max(time_of_first_included_data_point, rate_updates_[i].time);

    TimeDelta extra_time_needed =
        (is_last_update &&
         data_allocated_for_last_data_point < last_data_point.size)
            ? (last_data_point.size - data_allocated_for_last_data_point) /
                  update.rate
            : TimeDelta::Zero();

    Timestamp rate_update_end =
        (is_last_update ? time : rate_updates_[i + 1].time) + extra_time_needed;

    allocated_send_data_size +=
        (rate_update_end - rate_update_start) * update.rate;
  }

  return total_produced_data.bytes<double>() / allocated_send_data_size.bytes();
}
}  // namespace webrtc
