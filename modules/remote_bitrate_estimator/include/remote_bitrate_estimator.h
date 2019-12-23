/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This class estimates the incoming available bandwidth.

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

class Clock;

// RemoteBitrateObserver is used to signal changes in bitrate estimates for
// the incoming streams.
class RemoteBitrateObserver {
 public:
  // Called when a receive channel group has a new bitrate estimate for the
  // incoming streams.
  virtual void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                       uint32_t bitrate) = 0;

  virtual ~RemoteBitrateObserver() {}
};

class TransportFeedbackSenderInterface {
 public:
  virtual ~TransportFeedbackSenderInterface() = default;

  virtual bool SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets) = 0;
};

// TODO(holmer): Remove when all implementations have been updated.
struct ReceiveBandwidthEstimatorStats {};

// Rtp packet as seen by bandwidth estimation components.
struct BwePacket {
  int64_t arrival_time_ms;
  size_t payload_size;  // size of the potentially useful part of the packet.
  size_t total_size;    // size of the packet including overhead.
  uint32_t ssrc;
  uint32_t rtp_timestamp;
  absl::optional<int32_t> transmission_time_offset;
  absl::optional<uint32_t> absolute_send_time;
  absl::optional<uint16_t> transport_sequence_number;
  absl::optional<FeedbackRequest> feedback_request;
};

BwePacket ToBwePacket(const RtpPacketReceived& rtp_packet);
BwePacket ToBwePacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header);

class RemoteBitrateEstimator : public CallStatsObserver, public Module {
 public:
  ~RemoteBitrateEstimator() override {}

  // Called for each incoming packet. Updates the incoming payload bitrate
  // estimate and the over-use detector. If an over-use is detected the
  // remote bitrate estimate will be updated. Note that |payload_size| is the
  // packet size excluding headers.
  // Note that |arrival_time_ms| can be of an arbitrary time base.
  RTC_DEPRECATED
  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) {
    IncomingPacket(ToBwePacket(arrival_time_ms, payload_size, header));
  }
  virtual void IncomingPacket(const BwePacket& rtp_packet) = 0;

  // Removes all data for |ssrc|.
  virtual void RemoveStream(uint32_t ssrc) = 0;

  // Returns true if a valid estimate exists and sets |bitrate_bps| to the
  // estimated payload bitrate in bits per second. |ssrcs| is the list of ssrcs
  // currently being received and of which the bitrate estimate is based upon.
  virtual bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                              uint32_t* bitrate_bps) const = 0;

  // TODO(holmer): Remove when all implementations have been updated.
  virtual bool GetStats(ReceiveBandwidthEstimatorStats* output) const;

  virtual void SetMinBitrate(int min_bitrate_bps) = 0;

 protected:
  static const int64_t kProcessIntervalMs = 500;
  static const int64_t kStreamTimeOutMs = 2000;
};

inline bool RemoteBitrateEstimator::GetStats(
    ReceiveBandwidthEstimatorStats* output) const {
  return false;
}

}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_
