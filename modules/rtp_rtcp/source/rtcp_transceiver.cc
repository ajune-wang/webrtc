/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver.h"

#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

RtcpTransceiver::RtcpTransceiver(const RtcpTransceiverConfig& config)
    : task_queue_(config.task_queue),
      rtcp_transceiver_(rtc::MakeUnique<RtcpTransceiverImpl>(config)),
      ptr_factory_(rtcp_transceiver_.get()),
      // Creating first weak ptr can be done on any thread, but is not
      // thread-safe, thus do it at construction. Creating second (e.g. making a
      // copy) is thread-safe.
      ptr_(ptr_factory_.GetWeakPtr()) {
  RTC_DCHECK(task_queue_);
}

RtcpTransceiver::~RtcpTransceiver() {
  if (task_queue_->IsCurrent())
    return;

  rtc::Event done(false, false);
  task_queue_->PostTask([this, &done] {
    ptr_factory_.InvalidateWeakPtrs();
    rtcp_transceiver_.reset();
    done.Set();
  });
  // Wait until destruction is complete to be sure weak pointers invalidated and
  // rtcp_transceiver destroyed on the queue.
  done.Wait(rtc::Event::kForever);
}

void RtcpTransceiver::ReceivePacket(rtc::CopyOnWriteBuffer packet) {
  rtc::WeakPtr<RtcpTransceiverImpl> ptr = ptr_;
  int64_t now_us = rtc::TimeMicros();
  task_queue_->PostTask([ptr, packet, now_us] {
    if (ptr)
      ptr->ReceivePacket(packet, now_us);
  });
}

void RtcpTransceiver::SendCompoundPacket() {
  rtc::WeakPtr<RtcpTransceiverImpl> ptr = ptr_;
  task_queue_->PostTask([ptr] {
    if (ptr)
      ptr->SendCompoundPacket();
  });
}

void RtcpTransceiver::SetRemb(int bitrate_bps, std::vector<uint32_t> ssrcs) {
  struct SetRembClosure {
    void operator()() {
      if (ptr)
        ptr->SetRemb(bitrate_bps, std::move(ssrcs));
    }

    rtc::WeakPtr<RtcpTransceiverImpl> ptr;
    int bitrate_bps;
    std::vector<uint32_t> ssrcs;
  };
  task_queue_->PostTask(SetRembClosure{ptr_, bitrate_bps, std::move(ssrcs)});
}

void RtcpTransceiver::UnsetRemb() {
  rtc::WeakPtr<RtcpTransceiverImpl> ptr = ptr_;
  task_queue_->PostTask([ptr] {
    if (ptr)
      ptr->UnsetRemb();
  });
}

}  // namespace webrtc
