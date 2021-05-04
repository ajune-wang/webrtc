/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_ESTIMATOR_PROXY_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_ESTIMATOR_PROXY_H_

#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "api/transport/network_control.h"
#include "api/transport/webrtc_key_value_config.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class Clock;
namespace rtcp {
class TransportFeedback;
}

// PacketArrivalTimeMap is an optimized map of packet sequence number to arrival
// time, limited in size to never exceed `kMaxNumberOfPackets`. It will grow as
// needed, and remove old packets, and will expand to allow earlier packets to
// be added (out-of-order).
//
// Not yet received packets have the arrival time zero. The queue will not span
// larger than necessary and the last packet should always be received. The
// first packet in the queue doesn't have to be received in case of receiving
// packets out-of-order.
class PacketArrivalTimeMap {
 public:
  static constexpr size_t kMaxNumberOfPackets = 32768;

  // Indicates if the packet with `sequence_number` has already been received.
  bool has_received(int64_t sequence_number) const;

  // Returns the sequence number of the first entry in the map, i.e. the
  // sequence number that a `begin()` iterator would represent.
  int64_t begin_sequence_number() const { return begin_sequence_number_; }

  // Returns the sequence number of the element just after the map, i.e. the
  // sequence number that an `end()` iterator would represent.
  int64_t end_sequence_number() const {
    return begin_sequence_number_ + arrival_times.size();
  }

  // Returns an element by `sequence_number`, which must be valid, i.e.
  // between [begin_sequence_number, end_sequence_number).
  int64_t get(int64_t sequence_number) {
    int64_t pos = sequence_number - begin_sequence_number_;
    RTC_DCHECK(pos >= 0 && pos < static_cast<int64_t>(arrival_times.size()));
    return arrival_times[pos];
  }

  // Clamps `sequence_number` between [begin_sequence_number,
  // end_sequence_number].
  int64_t clamp(int64_t sequence_number) const;

  // Erases all elements from the beginning of the map until `sequence_number`.
  void EraseTo(int64_t sequence_number);

  // Records the fact that a packet with `sequence_number` arrived at
  // `arrival_time_ms`.
  void AddPacket(int64_t sequence_number, int64_t arrival_time_ms);

  // Removes packets from the beginning of the map as long as they are received
  // before `sequence_number` and with an age older than `arrival_time_limit`
  void RemoveOldPackets(int64_t sequence_number, int64_t arrival_time_limit);

 private:
  // Deque representing unwrapped sequence number -> time, where the index +
  // `begin_sequence_number_` represents the packet's sequence number.
  std::deque<int64_t> arrival_times;

  // The unwrapped sequence number for the first element in
  // `arrival_times`.
  int64_t begin_sequence_number_ = 0;
};

// Class used when send-side BWE is enabled: This proxy is instantiated on the
// receive side. It buffers a number of receive timestamps and then sends
// transport feedback messages back too the send side.
class RemoteEstimatorProxy : public RemoteBitrateEstimator {
 public:
  // Used for sending transport feedback messages when send side
  // BWE is used.
  using TransportFeedbackSender = std::function<void(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets)>;
  RemoteEstimatorProxy(Clock* clock,
                       TransportFeedbackSender feedback_sender,
                       const WebRtcKeyValueConfig* key_value_config,
                       NetworkStateEstimator* network_state_estimator);
  ~RemoteEstimatorProxy() override;

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override;
  void RemoveStream(uint32_t ssrc) override {}
  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override {}
  void SetMinBitrate(int min_bitrate_bps) override {}
  int64_t TimeUntilNextProcess() override;
  void Process() override;
  void OnBitrateChanged(int bitrate);
  void SetSendPeriodicFeedback(bool send_periodic_feedback);

 private:
  struct TransportWideFeedbackConfig {
    FieldTrialParameter<TimeDelta> back_window{"wind", TimeDelta::Millis(500)};
    FieldTrialParameter<TimeDelta> min_interval{"min", TimeDelta::Millis(50)};
    FieldTrialParameter<TimeDelta> max_interval{"max", TimeDelta::Millis(250)};
    FieldTrialParameter<TimeDelta> default_interval{"def",
                                                    TimeDelta::Millis(100)};
    FieldTrialParameter<double> bandwidth_fraction{"frac", 0.05};
    explicit TransportWideFeedbackConfig(
        const WebRtcKeyValueConfig* key_value_config) {
      ParseFieldTrial({&back_window, &min_interval, &max_interval,
                       &default_interval, &bandwidth_fraction},
                      key_value_config->Lookup(
                          "WebRTC-Bwe-TransportWideFeedbackIntervals"));
    }
  };

  void MaybeCullOldPackets(int64_t sequence_number, int64_t arrival_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&lock_);
  void SendPeriodicFeedbacks() RTC_EXCLUSIVE_LOCKS_REQUIRED(&lock_);
  void SendFeedbackOnRequest(int64_t sequence_number,
                             const FeedbackRequest& feedback_request)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&lock_);

  // Returns a Transport Feedback packet with information about as many packets
  // that has been received between [`begin_sequence_number_incl`,
  // `end_sequence_number_excl`) that can fit in it. If `is_periodic_update`,
  // this represents sending a periodic feedback message, which will make it
  // update the `periodic_window_start_seq_` variable with the first packet that
  // was not included in the feedback packet, so that the next update can
  // continue from that sequence number.
  //
  // If no incoming packets were added, nullptr
  // is returned.
  //
  // `include_timestamps` decide if the returned TransportFeedback should
  // include timestamps.
  std::unique_ptr<rtcp::TransportFeedback> BuildFeedbackPacket(
      bool include_timestamps,
      int64_t begin_sequence_number_inclusive,
      int64_t end_sequence_number_exclusive,
      bool is_periodic_update) RTC_EXCLUSIVE_LOCKS_REQUIRED(&lock_);

  Clock* const clock_;
  const TransportFeedbackSender feedback_sender_;
  const TransportWideFeedbackConfig send_config_;
  int64_t last_process_time_ms_;

  Mutex lock_;
  //  |network_state_estimator_| may be null.
  NetworkStateEstimator* const network_state_estimator_
      RTC_PT_GUARDED_BY(&lock_);
  uint32_t media_ssrc_ RTC_GUARDED_BY(&lock_);
  uint8_t feedback_packet_count_ RTC_GUARDED_BY(&lock_);
  SeqNumUnwrapper<uint16_t> unwrapper_ RTC_GUARDED_BY(&lock_);

  // The next sequence number that should be the start sequence number during
  // periodic reporting. Will be absl::nullopt before the first seen packet.
  absl::optional<int64_t> periodic_window_start_seq_ RTC_GUARDED_BY(&lock_);

  // Packet arrival times, by sequence number.
  PacketArrivalTimeMap packet_arrival_times_ RTC_GUARDED_BY(&lock_);

  int64_t send_interval_ms_ RTC_GUARDED_BY(&lock_);
  bool send_periodic_feedback_ RTC_GUARDED_BY(&lock_);

  // Unwraps absolute send times.
  uint32_t previous_abs_send_time_ RTC_GUARDED_BY(&lock_);
  Timestamp abs_send_timestamp_ RTC_GUARDED_BY(&lock_);
};

}  // namespace webrtc

#endif  //  MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_ESTIMATOR_PROXY_H_
