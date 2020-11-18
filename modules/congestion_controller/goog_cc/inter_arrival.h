/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_INTER_ARRIVAL_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_INTER_ARRIVAL_H_

#include <stddef.h>
#include <stdint.h>
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

// Helper class to compute the inter-arrival time delta and the size delta
// between two send bursts. This code is branched off
 // modules\remote_bitrate_estimator
class InterArrival {
 public:
  // After this many packet groups received out of order InterArrival will
  // reset, assuming that clocks have made a jump.
  static constexpr int kReorderedResetThreshold = 3;

  // A timestamp group is defined as all packets with a timestamp which are at
  // most timestamp_group_length_ticks older than the first timestamp in that
  // group.
  InterArrival(TimeDelta send_time_group_length);


  InterArrival() = delete;
  InterArrival(const InterArrival&) = delete;
  InterArrival& operator=(const InterArrival&) = delete;

  // This function returns true if a delta was computed, or false if the current
  // group is still incomplete or if only one group has been completed.
  // |timestamp| is the send time.
  // |arrival_time| is the local time at which the packet arrived.
  // |packet_size| is the size of the packet.
  // |timestamp_delta| (output) is the computed send time delta.
  // |arrival_time_delta_ms| (output) is the computed arrival-time delta.
  // |packet_size_delta| (output) is the computed size delta.
  bool ComputeDeltas(Timestamp send_time,
                     Timestamp arrival_time,
                     size_t packet_size,
                     TimeDelta* send_time_delta,
                     TimeDelta* arrival_time_delta,
                     int* packet_size_delta);

 private:
  struct SendTimeGroup {
    SendTimeGroup()
        : size(0),
          first_send_time(Timestamp::MinusInfinity()),
          send_time(Timestamp::MinusInfinity()),
          first_arrival(Timestamp::MinusInfinity()),
          complete_time(Timestamp::MinusInfinity()) {}

    bool IsFirstPacket() const { return complete_time.IsInfinite(); }

    size_t size;
    Timestamp first_send_time;
    Timestamp send_time;
    Timestamp first_arrival;
    Timestamp complete_time;
  };


  bool PacketInOrder(Timestamp send_time);

  // Returns true if the last packet was the end of the current batch and the
  // packet with |send_time| is the first of a new batch.
  bool NewTimestampGroup(Timestamp arrival_time, Timestamp send_time) const;

  bool BelongsToBurst(Timestamp arrival_time, Timestamp send_time) const;

  void Reset();

  const TimeDelta send_time_group_length_;
  SendTimeGroup current_timestamp_group_;
  SendTimeGroup prev_timestamp_group_;
  int num_consecutive_reordered_packets_;
};
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_INTER_ARRIVAL_H_
