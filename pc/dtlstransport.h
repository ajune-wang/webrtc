/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DTLSTRANSPORT_H_
#define PC_DTLSTRANSPORT_H_

#include <memory>

#include "api/dtlstransportinterface.h"
#include "p2p/base/dtlstransport.h"
#include "rtc_base/asyncinvoker.h"

namespace webrtc {

// This implementation wraps a cricket::DtlsTransport, and takes
// ownership of it.
class DtlsTransport : public DtlsTransportInterface,
                      public sigslot::has_slots<> {
 public:
  explicit DtlsTransport(
      std::unique_ptr<cricket::DtlsTransportInternal> internal);

  void RegisterObserver(DtlsTransportObserverInterface* observer) override;
  void UnregisterObserver() override;

  cricket::DtlsTransportInternal* internal() {
    return internal_dtls_transport_.get();
  }
  void clear() { internal_dtls_transport_.reset(); }
  // Report a state change to observer.
  // Must be called on thread that owns internal_dtls_transport_
  void dispatchStateChange(bool real_change);

 private:
  void OnInternalDtlsState(cricket::DtlsTransportInternal* transport,
                           cricket::DtlsTransportState state);

  DtlsTransportObserverInterface* observer_;
  rtc::Thread* thread_owning_internal_;
  std::unique_ptr<cricket::DtlsTransportInternal> internal_dtls_transport_;
  rtc::AsyncInvoker invoker_;
};

}  // namespace webrtc
#endif  // PC_DTLSTRANSPORT_H_
