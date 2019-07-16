/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_NETWORK_ESTIMATE_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_NETWORK_ESTIMATE_H_

#include <memory>
#include <vector>

#include "api/transport/network_types.h"
#include "modules/rtp_rtcp/source/rtcp_packet/app.h"

namespace webrtc {
namespace rtcp {

class CommonHeader;
template <typename StructType>
class Serializer {
 public:
  virtual void Parse(rtc::ArrayView<const uint8_t> src, StructType* target) = 0;
  virtual rtc::Buffer Serialize(const StructType& src) = 0;
  virtual ~Serializer() = default;
};

class NetworkEstimate : public App {
 public:
  NetworkEstimate();
  static constexpr uint8_t kSubType = 13;
  // static constexpr uint8_t kName[4]  = {'g','o','o','g'};
  static constexpr uint32_t kName = 0x676f6f67;  // "goog"
  static TimeDelta GetTimestampPeriod();

  static bool IsNetworkEstimate(const CommonHeader& packet);
  bool Parse(const CommonHeader& packet);
  void SetEstimate(NetworkStateEstimate estimate);
  NetworkStateEstimate estimate() const { return estimate_; }

 private:
  NetworkStateEstimate estimate_;
  Serializer<NetworkStateEstimate>* serializer_;
};

}  // namespace rtcp
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_NETWORK_ESTIMATE_H_
