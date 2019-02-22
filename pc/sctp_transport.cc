/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_transport.h"

#include <utility>

#include "pc/ice_transport.h"

namespace webrtc {

SctpTransport::SctpTransport(
    std::unique_ptr<cricket::SctpTransportInternal> internal,
    rtc::scoped_refptr<DtlsTransport> dtls_transport)
    : owner_thread_(rtc::Thread::Current()),
      info_(SctpTransportState::kNew),
      internal_sctp_transport_(std::move(internal)),
      dtls_transport_(std::move(dtls_transport)) {
  RTC_DCHECK(internal_sctp_transport_.get());
  internal_sctp_transport_->SignalReadyToSendData.connect(
      this, &SctpTransport::OnInternalReadyToSendData);
  internal_sctp_transport_->SignalClosingProcedureStartedRemotely.connect(
      this, &SctpTransport::OnInternalClosingProcedureStartedRemotely);
  internal_sctp_transport_->SignalClosingProcedureComplete.connect(
      this, &SctpTransport::OnInternalClosingProcedureComplete);

  if (dtls_transport_) {
    UpdateInformation(SctpTransportState::kConnecting);
  } else {
    UpdateInformation(SctpTransportState::kNew);
  }
}

SctpTransport::~SctpTransport() {
  // We depend on the network thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(owner_thread_->IsCurrent() || !internal_sctp_transport_);
}

SctpTransportInformation SctpTransport::Information() {
  rtc::CritScope scope(&lock_);
  return info_;
}

void SctpTransport::RegisterObserver(SctpTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  observer_ = observer;
}

void SctpTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

rtc::scoped_refptr<DtlsTransportInterface> SctpTransport::dtls_transport() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  rtc::CritScope scope(&lock_);
  return dtls_transport_;
}

// Internal functions
void SctpTransport::Clear() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal());
  bool must_send_event = true;
  // TODO(hta): Check against already-closed state
  {
    rtc::CritScope scope(&lock_);
    dtls_transport_ = nullptr;
    internal_sctp_transport_ = nullptr;
  }
  UpdateInformation(SctpTransportState::kClosed);
  if (observer_ && must_send_event) {
    observer_->OnStateChange(Information());
  }
}

void SctpTransport::SetDtlsTransport(
    rtc::scoped_refptr<DtlsTransport> transport) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  rtc::CritScope scope(&lock_);
  dtls_transport_ = transport;
  if (internal_sctp_transport_) {
    if (transport) {
      internal_sctp_transport_->SetDtlsTransport(transport->internal());
      if (info_.state() == SctpTransportState::kNew) {
        UpdateInformation(SctpTransportState::kConnecting);
      }
    } else {
      internal_sctp_transport_->SetDtlsTransport(nullptr);
    }
  }
}

void SctpTransport::UpdateInformation(SctpTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_LOG(LS_ERROR) << "DEBUG: SctpTransportState changed to "
                    << static_cast<int>(state);
  rtc::CritScope scope(&lock_);
  bool must_send_update = (state != info_.state());
  info_ = SctpTransportInformation(state);
  if (observer_ && must_send_update) {
    observer_->OnStateChange(info_);
  }
}

void SctpTransport::OnInternalReadyToSendData() {
  UpdateInformation(SctpTransportState::kConnected);
}

void SctpTransport::OnInternalClosingProcedureStartedRemotely(int sid) {
  UpdateInformation(SctpTransportState::kClosed);
}

void SctpTransport::OnInternalClosingProcedureComplete(int sid) {
  UpdateInformation(SctpTransportState::kClosed);
}

}  // namespace webrtc
