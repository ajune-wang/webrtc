/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {
class ReceiveStatisticsProvider;
class Transport;

//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is not thread-safe.
class RtcpTransceiverImpl {
 public:
  explicit RtcpTransceiverImpl(const RtcpTransceiver::Configuration& config);
  ~RtcpTransceiverImpl();

  // Process incoming rtcp packet.
  void ReceivePacket(rtc::ArrayView<const uint8_t> packet);

  // Sends sender/receiver report asap.
  void ForceSendReport();
  int64_t TimeUntilNextPeriodicReport() const;

 private:
  struct LastSenderReport {
    int64_t local_time_us;
    uint32_t remote_compact_ntp_time;
  };
  void SchedulePeriodicReport(int64_t delay_ms);
  // Sends sender/receiver.
  // Returns recommended time until next report in milliseconds.
  int64_t SendReport();

  const RtcpTransceiver::Configuration config_;
  int64_t next_report_ms_ = 0;  // infinite past initially.

  std::map<uint32_t, LastSenderReport> remote_senders_;

  std::unique_ptr<rtc::WeakPtrFactory<RtcpTransceiverImpl>> weak_ptr_factory_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiverImpl);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
