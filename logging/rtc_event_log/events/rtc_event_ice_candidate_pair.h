/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_ICE_CANDIDATE_PAIR_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_ICE_CANDIDATE_PAIR_H_

#include "logging/rtc_event_log/events/rtc_event.h"

#include <string>

namespace webrtc {

enum class IceCandidatePairEventType {
  kPruned,
  kAdded,
  kCheckSent,
  kCheckReceived,
  kCheckResponseSent,
  kCheckResponseReceived,
  kSelected,
};

enum class IceCandidatePairContentName {
  kAudio,
  kVideo,
  kData,
  kUnknown,
};

enum class IceCandidateType {
  kLocal,
  kStun,
  kPrflx,
  kRelay,
  kUnknown,
};

enum class IceCandidatePairProtocol {
  kUdp,
  kTcp,
  kSsltcp,
  kTls,
  kUnknown,
};

enum class IceCandidatePairAddressFamily {
  kIpv4,
  kIpv6,
  kUnknown,
};

enum class IceCandidateNetworkType {
  kEthernet,
  kLoopback,
  kWifi,
  kVpn,
  kCellular,
  kUnknown,
};

class IceCandidatePairDescription {
 public:
  IceCandidatePairDescription();
  explicit IceCandidatePairDescription(
      const IceCandidatePairDescription& other);

  ~IceCandidatePairDescription();

  IceCandidatePairContentName content_name;
  IceCandidateType local_candidate_type;
  IceCandidateNetworkType local_network_type;
  IceCandidateType remote_candidate_type;
  IceCandidatePairProtocol candidate_pair_protocol;
  IceCandidatePairAddressFamily candidate_pair_address_family;
};

class RtcEventIceCandidatePair final : public RtcEvent {
 public:
  RtcEventIceCandidatePair(
      IceCandidatePairEventType type,
      uint32_t candidate_pair_id,
      const IceCandidatePairDescription& candidate_pair_desc);

  ~RtcEventIceCandidatePair() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  const IceCandidatePairEventType type_;
  const uint32_t candidate_pair_id_;
  const IceCandidatePairDescription candidate_pair_desc_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_ICE_CANDIDATE_PAIR_H_
