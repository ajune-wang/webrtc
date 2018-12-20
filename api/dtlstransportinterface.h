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

class DtlsTransportObserverInterface {
 public:
  // This callback carries information about the state of the transport.
  // Called (with state_changed=false) as soon as possible after
  // the observer is set.
  // Called subsequently (with state_changed=true) for every change in state
  // of the DTLSTransport.
  virtual void OnStateChange(DtlsTransportState state,
                             bool state_changed,
                             void* remoteCertificates) = 0;
  // This callback is called when an error occurs, causing the transport
  // to go to the kFailed state.
  virtual void OnError(RTCError error) = 0;

 protected:
  virtual ~DtlsTransportObserverInterface() = default;
};

// A DTLS transport, as represented to the outside world.
// Its role is to report state changes and errors, and make sure information
// about remote certificates is available.
// By design, the API has no accessors; all information is carried back
// through the observer.
// When the observer is registered, an immediate callback is dispatched
// with state_changed = false, so that the current state is observable.
class DtlsTransportInterface : public rtc::RefCountInterface {
 public:
  virtual void RegisterObserver(DtlsTransportObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;
};

}  // namespace webrtc

#endif  // API_DTLSTRANSPORTINTERFACE_H_
