/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_TRANSPORT_H_
#define PC_SCTP_TRANSPORT_H_

#include <memory>

#include "api/ice_transport_interface.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "media/sctp/sctp_transport.h"
#include "pc/dtls_transport.h"
#include "rtc_base/async_invoker.h"

namespace webrtc {

class IceTransportWithPointer;

// This implementation wraps a cricket::SctpTransport, and takes
// ownership of it.
class SctpTransport : public SctpTransportInterface,
                      public sigslot::has_slots<> {
 public:
  // This object must be constructed and updated on a consistent thread,
  // the same thread as the one the cricket::SctpTransportInternal object
  // lives on.
  // The Information() function can be called from a different thread,
  // such as the signalling thread.
  SctpTransport(std::unique_ptr<cricket::SctpTransportInternal> internal,
                rtc::scoped_refptr<DtlsTransport> dtls_transport);

  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() override;
  SctpTransportInformation Information() override;
  void RegisterObserver(SctpTransportObserverInterface* observer) override;
  void UnregisterObserver() override;

  void Clear();
  void SetDtlsTransport(rtc::scoped_refptr<DtlsTransport>);

  cricket::SctpTransportInternal* internal() {
    rtc::CritScope scope(&lock_);
    return internal_sctp_transport_.get();
  }

  const cricket::SctpTransportInternal* internal() const {
    rtc::CritScope scope(&lock_);
    return internal_sctp_transport_.get();
  }

 protected:
  ~SctpTransport();

 private:
  void UpdateInformation(SctpTransportState state);
  void OnInternalReadyToSendData();
  void OnInternalClosingProcedureStartedRemotely(int sid);
  void OnInternalClosingProcedureComplete(int sid);

  SctpTransportObserverInterface* observer_ = nullptr;
  rtc::Thread* owner_thread_;
  rtc::CriticalSection lock_;
  SctpTransportInformation info_ RTC_GUARDED_BY(lock_);
  std::unique_ptr<cricket::SctpTransportInternal> internal_sctp_transport_
      RTC_GUARDED_BY(lock_);
  rtc::scoped_refptr<DtlsTransport> dtls_transport_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc
#endif  // PC_SCTP_TRANSPORT_H_
