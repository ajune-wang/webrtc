/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"

#include <utility>
#include <vector>

#include "api/call/transport.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/checks.h"

namespace webrtc {

RtcpTransceiverImpl::RtcpTransceiverImpl(const RtcpTransceiverConfig& config)
    : config_(config) {
  RTC_CHECK(config_.Validate());
}

RtcpTransceiverImpl::~RtcpTransceiverImpl() = default;

void RtcpTransceiverImpl::SendCompoundPacket() {
  rtcp::CompoundPacket compound;

  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(config_.feedback_ssrc);
  if (config_.receive_statistics) {
    // TODO(danilchap): Support sending more than
    // |ReceiverReport::kMaxNumberOfReportBlocks| per compound rtcp packet.
    std::vector<rtcp::ReportBlock> report_blocks =
        config_.receive_statistics->RtcpReportBlocks(
            rtcp::ReceiverReport::kMaxNumberOfReportBlocks);
    // TODO(danilchap): Fill in LastSr/DelayLastSr fields of report blocks
    // when RtcpTransceiver handles incoming sender reports.
    rr.SetReportBlocks(std::move(report_blocks));
  }
  compound.Append(&rr);
  // TODO(danilchap): Append SDES to conform to the requirements on minimal
  // compound RTCP packet.

  size_t max_packet_size = config_.max_packet_size;
  compound.Build(max_packet_size, [&](rtc::ArrayView<const uint8_t> packet) {
    config_.outgoing_transport->SendRtcp(packet.data(), packet.size());
  });
}

}  // namespace webrtc
