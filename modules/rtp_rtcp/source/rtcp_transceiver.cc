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

RtcpTransceiver::RtcpTransceiver(rtc::TaskQueue* task_queue,
                                 const Configuration& config)
    : task_queue_(task_queue) {
  auto construct = [this, &config] {
    rtcp_transceiver_ = rtc::MakeUnique<RtcpTransceiverImpl>(config);
    ptr_factory_ = rtc::MakeUnique<rtc::WeakPtrFactory<RtcpTransceiverImpl>>(
        rtcp_transceiver_.get());
    ptr_ = ptr_factory_->GetWeakPtr();
  };

  if (task_queue_->IsCurrent()) {
    construct();
    return;
  }

  rtc::Event done(false, false);
  task_queue_->PostTask(rtc::NewClosure(construct,
                                        /*cleanup=*/[&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
  RTC_CHECK(rtcp_transceiver_) << "Task queue is too busy to handle rtcp";
}

RtcpTransceiver::~RtcpTransceiver() {
  auto destruct = [this] {
    ptr_factory_.reset();
    rtcp_transceiver_.reset();
  };
  if (task_queue_->IsCurrent()) {
    destruct();
    return;
  }

  rtc::Event done(false, false);
  task_queue_->PostTask(rtc::NewClosure(destruct,
                                        /*cleanup=*/[&done] { done.Set(); }));
  done.Wait(rtc::Event::kForever);
  RTC_CHECK(!rtcp_transceiver_) << "Task queue is too busy to handle rtcp";
}

void RtcpTransceiver::ReceivePacket(rtc::CopyOnWriteBuffer packet) {
  auto ptr = ptr_;
  task_queue_->PostTask([ptr, packet] {
    if (ptr)
      ptr->ReceivePacket(packet);
  });
}

void RtcpTransceiver::ForceSendReport() {
  auto ptr = ptr_;
  task_queue_->PostTask([ptr] {
    if (ptr)
      ptr->ForceSendReport();
  });
}

}  // namespace webrtc
