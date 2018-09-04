/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FAKE_MDNS_RESPONDER_H_
#define RTC_BASE_FAKE_MDNS_RESPONDER_H_

#include <map>
#include <memory>
#include <string>

#include "rtc_base/mdns_responder_interface.h"

#include "rtc_base/helpers.h"

namespace webrtc {

class FakeMDnsResponder : public MDnsResponderInterface {
 public:
  FakeMDnsResponder() = default;
  ~FakeMDnsResponder() = default;

  void CreateNameForAddress(
      const rtc::IPAddress& addr,
      std::unique_ptr<CallbackOnNameCreated> callback) override {
    std::string name = rtc::CreateRandomUuid() + ".local.";
    for (const auto& p : name_addr_map_) {
      if (p.second == addr) {
        name = p.first;
      }
    }
    name_addr_map_[name] = addr;
    callback->Run(addr, name);
  }

 private:
  std::map<std::string, rtc::IPAddress> name_addr_map_;
};

}  // namespace webrtc

#endif  // RTC_BASE_FAKE_MDNS_RESPONDER_H_
