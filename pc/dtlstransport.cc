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

// Implementation of DtlsTransportInterface
DtlsTransport::DtlsTransport(
    std::unique_ptr<cricket::DtlsTransportInternal> internal)
    : thread_owning_internal_(rtc::Thread::Current()),
      internal_dtls_transport_(std::move(internal)) {
  RTC_DCHECK(internal_dtls_transport_.get());
  internal_dtls_transport_->SignalDtlsState.connect(
      this, &DtlsTransport::OnInternalDtlsState);
}

DtlsTransportInformation DtlsTransport::Information() {
  RTC_DCHECK(thread_owning_internal_->IsCurrent());
  if (internal()) {
    return DtlsTransportInformation(TranslateState(internal()->dtls_state()));
  } else {
    return DtlsTransportInformation(DtlsTransportState::kClosed);
  }
}

void DtlsTransport::RegisterObserver(DtlsTransportObserverInterface* observer) {
  RTC_DCHECK(observer);
  if (!thread_owning_internal_->IsCurrent()) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, thread_owning_internal_,
        Bind(&DtlsTransport::RegisterObserver, this, observer));
    return;
  }
  observer_ = observer;
}

void DtlsTransport::UnregisterObserver() {
  if (!thread_owning_internal_->IsCurrent()) {
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_owning_internal_,
                               Bind(&DtlsTransport::UnregisterObserver, this));
    return;
  }
  observer_ = nullptr;
}

// Internal functions

void DtlsTransport::OnInternalDtlsState(
    cricket::DtlsTransportInternal* transport,
    cricket::DtlsTransportState state) {
  RTC_DCHECK(transport == internal());
  RTC_DCHECK(thread_owning_internal_->IsCurrent());
  RTC_DCHECK(state == internal()->dtls_state());
  if (observer_) {
    observer_->OnStateChange(Information());
  }
}

}  // namespace webrtc
