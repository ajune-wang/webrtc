/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_DTLSTRANSPORTINTERFACE_H_
#define API_DTLSTRANSPORTINTERFACE_H_

#include "api/rtcerror.h"
#include "rtc_base/refcount.h"

namespace webrtc {

// States of a DTLS transport, corresponding to the JS API specification.
// http://w3c.github.io/webrtc-pc/#dom-rtcdtlstransportstate
enum class DtlsTransportState {
  kNew,         // Has not started negotiating yet.
  kConnecting,  // In the process of negotiating a secure connection.
  kConnected,   // Completed negotiation and verified fingerprints.
  kClosed,      // Intentionally closed.
  kFailed  // Failure due to an error or not verifying a remote fingerprint.
};

// This object gives information about the state of a DTLSTransport.
// It can only be accessed from the same thread as the one that the
// Observer callback interfaces are called on.
// Calling its accessors will not cause thread jumps.
class DtlsTransportInformation {
 public:
  virtual DtlsTransportState State() const = 0;
  // TODO(hta): Add remote certificate access
  virtual ~DtlsTransportInformation() = default;
};

class DtlsTransportObserverInterface {
 public:
  // This callback carries information about the state of the transport.
  // The argument can only be accessed on the same thread as that which
  // the callback is called on.
  virtual void OnStateChange(const DtlsTransportInformation* info) = 0;
  // This callback is called when an error occurs, causing the transport
  // to go to the kFailed state.
  virtual void OnError(RTCError error) = 0;

 protected:
  virtual ~DtlsTransportObserverInterface() = default;
};

// A DTLS transport, as represented to the outside world.
// Its role is to report state changes and errors, and make sure information
// about remote certificates is available.
// Synchronous access to data is limited to the information() function,
// which has restrictions on access.
class DtlsTransportInterface : public rtc::RefCountInterface {
 public:
  // Access to information. This can only be accessed from the thread
  // that is used for callbacks from the implementation.
  virtual DtlsTransportInformation* information() = 0;
  // Observer management. This can be called from the client thread.
  virtual void RegisterObserver(DtlsTransportObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;
};

}  // namespace webrtc

#endif  // API_DTLSTRANSPORTINTERFACE_H_
