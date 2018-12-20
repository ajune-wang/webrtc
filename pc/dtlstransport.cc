/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtlstransport.h"

#include <utility>

namespace webrtc {

namespace {

DtlsTransportState TranslateState(cricket::DtlsTransportState internal_state) {
  switch (internal_state) {
    case cricket::DTLS_TRANSPORT_NEW:
      return DtlsTransportState::kNew;
      break;
    case cricket::DTLS_TRANSPORT_CONNECTING:
      return DtlsTransportState::kConnecting;
      break;
    case cricket::DTLS_TRANSPORT_CONNECTED:
      return DtlsTransportState::kConnected;
      break;
    case cricket::DTLS_TRANSPORT_CLOSED:
      return DtlsTransportState::kClosed;
      break;
    case cricket::DTLS_TRANSPORT_FAILED:
      return DtlsTransportState::kFailed;
      break;
  }
}

}  // namespace

DtlsTransport::DtlsTransport(
    std::unique_ptr<cricket::DtlsTransportInternal> internal)
    : thread_owning_internal_(rtc::Thread::Current()),
      internal_dtls_transport_(std::move(internal)) {
  RTC_DCHECK(internal_dtls_transport_.get());
  internal_dtls_transport_->SignalDtlsState.connect(
      this, &DtlsTransport::OnInternalDtlsState);
}

void DtlsTransport::OnInternalDtlsState(
    cricket::DtlsTransportInternal* transport,
    cricket::DtlsTransportState state) {
  RTC_DCHECK(transport == internal());
  DispatchStateChange(true);
}

void DtlsTransport::RegisterObserver(DtlsTransportObserverInterface* observer) {
  RTC_DCHECK(observer);
  observer_ = observer;
  if (!thread_owning_internal_->IsCurrent()) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, thread_owning_internal_,
        Bind(&DtlsTransport::DispatchStateChange, this, false));
    return;
  }
  // need to dispatch to signalling thread to safely access internal
  DispatchStateChange(false);
}

void DtlsTransport::UnregisterObserver() {
  observer_ = nullptr;
}

void DtlsTransport::DispatchStateChange(bool real_change) {
  RTC_DCHECK(thread_owning_internal_->IsCurrent());
  if (observer_) {
    if (internal()) {
      observer_->OnStateChange(TranslateState(internal()->dtls_state()),
                               real_change, nullptr);
    } else {
      observer_->OnStateChange(DtlsTransportState::kClosed, real_change,
                               nullptr);
    }
  }
}

}  // namespace webrtc
