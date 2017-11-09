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

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

RtcpTransceiver::RtcpTransceiver(const RtcpTransceiverConfig& config)
    : task_queue_(config.task_queue)
#if WEAKER_WEAK_PTR
      ,
      rtcp_transceiver_(rtc::MakeUnique<RtcpTransceiverImpl>(config)),
      ptr_factory_(rtcp_transceiver_.get()) {
}
#else
{
  RTC_DCHECK(task_queue_);
  auto construct = [this, &config] {
    rtcp_transceiver_ = rtc::MakeUnique<RtcpTransceiverImpl>(config);
    ptr_factory_.emplace(rtcp_transceiver_.get());
    ptr_ = ptr_factory_->GetWeakPtr();
  };

  if (task_queue_->IsCurrent()) {
    construct();
    return;
  }

  rtc::Event done(false, false);
  task_queue_->PostTask(rtc::NewClosure(construct,
                                        /*cleanup=*/[&done] { done.Set(); }));
  // Wait until construction is complete to be sure ptr_ is constructed on the
  // task queue.
  done.Wait(rtc::Event::kForever);
  RTC_CHECK(rtcp_transceiver_);
}
#endif

RtcpTransceiver::~RtcpTransceiver() {
  auto destruct = [this] {
#if WEAKER_WEAK_PTR
    ptr_factory_.InvalidateWeakPtrs();
#else
    ptr_factory_->InvalidateWeakPtrs();
// ptr_factory_.reset();
#endif
    rtcp_transceiver_.reset();
  };
  if (task_queue_->IsCurrent()) {
    destruct();
    return;
  }

  rtc::Event done(false, false);
  task_queue_->PostTask(rtc::NewClosure(destruct,
                                        /*cleanup=*/[&done] { done.Set(); }));
  // Wait until destruction is complete to be sure ptr_factory reset on the
  // queue.
  done.Wait(rtc::Event::kForever);
  RTC_CHECK(!rtcp_transceiver_);
}

void RtcpTransceiver::SendCompoundPacket() {
#if WEAKER_WEAK_PTR
  auto ptr = ptr_factory_.GetWeakPtr();
#else
  auto ptr = ptr_;
#endif
  task_queue_->PostTask([ptr] {
    if (ptr)
      ptr->SendCompoundPacket();
  });
}

}  // namespace webrtc
