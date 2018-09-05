/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MDNS_RESPONDER_INTERFACE_H_
#define RTC_BASE_MDNS_RESPONDER_INTERFACE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "rtc_base/ipaddress.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace webrtc {

class MDnsResponderInterface {
 public:
  class CallbackOnNameCreated {
   public:
    virtual ~CallbackOnNameCreated() = default;
    virtual void Run(const rtc::IPAddress& addr, const std::string& name) = 0;
  };

  MDnsResponderInterface() = default;
  virtual ~MDnsResponderInterface() = default;

  // Asynchronously creates a type-4 UUID hostname for an IP address. The
  // created name should be given to |callback| with the address that it
  // represents.
  virtual void CreateNameForAddress(
      const rtc::IPAddress& addr,
      std::unique_ptr<CallbackOnNameCreated> callback) {}
  // Clears the name association for the given address if there is such an
  // association previously created via CreateNameForAddress.
  virtual void RemoveNameForAddress(const rtc::IPAddress& addr) {}
};

}  // namespace webrtc

#endif  // RTC_BASE_MDNS_RESPONDER_INTERFACE_H_
