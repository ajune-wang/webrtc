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

enum class DtlsTransportState {
  kNew,
  kConnecting,
  kConnected,
  kClosed,
  kFailed
};

class DtlsTransportObserverInterface {
 public:
  virtual void OnStateChange(DtlsTransportState state,
                             bool state_changed,
                             void* remoteCertificates) = 0;
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
