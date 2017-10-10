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

#include "api/call/transport.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace {

// Helper to generate compound rtcp packets.
class PacketSender : public rtcp::RtcpPacket::PacketReadyCallback {
 public:
  PacketSender(Transport* transport, size_t max_packet_size)
      : transport_(transport), max_packet_size_(max_packet_size) {
    RTC_CHECK_LE(max_packet_size, IP_PACKET_SIZE);
  }
  ~PacketSender() { RTC_DCHECK_EQ(index_, 0) << "Unsent rtcp packet."; }

  void AddBlock(const rtcp::RtcpPacket& block) {
    block.Create(buffer_, &index_, max_packet_size_, this);
  }

  void Send() {
    if (index_ > 0) {
      OnPacketReady(buffer_, index_);
      index_ = 0;
    }
  }

 private:
  // Implements RtcpPacket::PacketReadyCallback
  void OnPacketReady(uint8_t* data, size_t length) override {
    transport_->SendRtcp(data, length);
  }

  Transport* const transport_;
  const size_t max_packet_size_;
  size_t index_ = 0;
  uint8_t buffer_[IP_PACKET_SIZE];
};

}  // namespace

RtcpTransceiver::Configuration::Configuration() = default;
RtcpTransceiver::Configuration::~Configuration() = default;

bool RtcpTransceiver::Configuration::Valid() const {
  if (feedback_ssrc == 0)
    LOG(LS_WARNING)
        << debug_id
        << "Ssrc 0 may be treated by some implementation as invalid.";
  if (cname.size() > 255) {
    LOG(LS_ERROR) << debug_id << "cname can be maximum 255 characters.";
    return false;
  }
  if (max_packet_size < 100) {
    LOG(LS_ERROR) << debug_id << "max packet size " << max_packet_size
                  << " is too small.";
    return false;
  }
  if (max_packet_size > IP_PACKET_SIZE) {
    LOG(LS_ERROR) << debug_id << "max packet size " << max_packet_size
                  << " more than " << IP_PACKET_SIZE << " is unsupported.";
    return false;
  }
  if (outgoing_transport == nullptr) {
    LOG(LS_ERROR) << debug_id << "outgoing transport must be set";
    return false;
  }
  if (min_periodic_report_ms <= 0) {
    LOG(LS_ERROR) << debug_id << "period " << min_periodic_report_ms
                  << "ms between reports should be positive.";
    return false;
  }
  if (receive_statistics == nullptr)
    LOG(LS_WARNING)
        << debug_id
        << "receive statistic should be set to generate rtcp report blocks.";
  return true;
}

RtcpTransceiverImpl::RtcpTransceiverImpl(
    const RtcpTransceiver::Configuration& config)
    : config_(config) {
  RTC_CHECK(config_.Valid());
  if (config_.schedule_periodic_reports)
    SchedulePeriodicReport(/*delay_ms=*/0);
}

RtcpTransceiverImpl::~RtcpTransceiverImpl() = default;

// Process incoming rtcp packet.
void RtcpTransceiverImpl::ReceivePacket(rtc::ArrayView<const uint8_t> packet) {
  rtcp::CommonHeader rtcp_block;
  const uint8_t* const packet_begin = packet.data();
  const uint8_t* const packet_end = packet.data() + packet.size();
  for (const uint8_t* next_block = packet_begin; next_block != packet_end;
       next_block = rtcp_block.NextPacket()) {
    ptrdiff_t remaining_blocks_size = packet_end - next_block;
    RTC_DCHECK_GT(remaining_blocks_size, 0);
    if (!rtcp_block.Parse(next_block, remaining_blocks_size))
      break;

    switch (rtcp_block.type()) {
      case rtcp::SenderReport::kPacketType: {
        rtcp::SenderReport sr;
        if (!sr.Parse(rtcp_block))
          break;
        LastSenderReport& last_report = remote_senders_[sr.sender_ssrc()];
        last_report.local_time_us = rtc::TimeMicros();
        last_report.remote_compact_ntp_time = CompactNtp(sr.ntp());
        break;
      }
    }
  }
}

void RtcpTransceiverImpl::ForceSendReport() {
  int64_t delay_ms = SendReport();
  if (config_.schedule_periodic_reports)
    // Stop and restart the scheduled report.
    SchedulePeriodicReport(delay_ms);
}

int64_t RtcpTransceiverImpl::TimeUntilNextPeriodicReport() const {
  return next_report_ms_ - rtc::TimeMillis();
}

void RtcpTransceiverImpl::SchedulePeriodicReport(int64_t delay_ms) {
  RTC_DCHECK(config_.schedule_periodic_reports);
  class PeriodicReport : public rtc::QueuedTask {
   public:
    explicit PeriodicReport(rtc::WeakPtr<RtcpTransceiverImpl> ptr)
        : ptr_(std::move(ptr)) {}
    bool Run() override {
      if (!ptr_)
        return true;
      int64_t delay_ms = ptr_->SendReport();
      RTC_DCHECK_GT(delay_ms, 0);
      rtc::TaskQueue::Current()->PostDelayedTask(
          std::unique_ptr<QueuedTask>(this), delay_ms);
      return false;
    }

   private:
    const rtc::WeakPtr<RtcpTransceiverImpl> ptr_;
  };

  rtc::TaskQueue* task_queue = rtc::TaskQueue::Current();
  RTC_DCHECK(task_queue) << "Need to be running on a queue";
  // Stop existent report if there is one.
  weak_ptr_factory_ =
      rtc::MakeUnique<rtc::WeakPtrFactory<RtcpTransceiverImpl>>(this);
  std::unique_ptr<rtc::QueuedTask> task =
      rtc::MakeUnique<PeriodicReport>(weak_ptr_factory_->GetWeakPtr());
  if (delay_ms > 0)
    task_queue->PostDelayedTask(std::move(task), delay_ms);
  else
    task_queue->PostTask(std::move(task));
}

int64_t RtcpTransceiverImpl::SendReport() {
  PacketSender sender(config_.outgoing_transport, config_.max_packet_size);

  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(config_.feedback_ssrc);
  if (config_.receive_statistics) {
    // TODO(danilchap): Support sending more than
    // |ReceiverReport::kMaxNumberOfReportBlocks| per compound rtcp packet.
    auto report_blocks = config_.receive_statistics->RtcpReportBlocks(
        rtcp::ReceiverReport::kMaxNumberOfReportBlocks);

    for (rtcp::ReportBlock& rb : report_blocks) {
      auto rs = remote_senders_.find(rb.source_ssrc());
      if (rs != remote_senders_.end()) {
        // Compact ntp time wraps around every 18.2 hours.
        int64_t delay_us = rtc::TimeMicros() - rs->second.local_time_us;
        int64_t delay = delay_us * (1u << 16) / 1000000;
        rb.SetLastSr(rs->second.remote_compact_ntp_time);
        rb.SetDelayLastSr(rtc::saturated_cast<uint32_t>(delay));
      }
    }

    rr.SetReportBlocks(std::move(report_blocks));
  }
  sender.AddBlock(rr);

  if (!config_.cname.empty()) {
    rtcp::Sdes sdes;
    bool added = sdes.AddCName(config_.feedback_ssrc, config_.cname);
    RTC_DCHECK(added) << "Failed to add cname " << config_.cname
                      << " to rtcp sdes packet.";
    sender.AddBlock(sdes);
  }
  sender.Send();
  return config_.min_periodic_report_ms;
}

}  // namespace webrtc
