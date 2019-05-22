/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MOCK_MDNS_RESPONDER_H_
#define RTC_BASE_MOCK_MDNS_RESPONDER_H_

#include "rtc_base/fake_mdns_responder.h"

namespace webrtc {

class MockMdnsResponder : public webrtc::MdnsResponderInterface {
 public:
  MockMdnsResponder() = default;
  ~MockMdnsResponder() = default;

  MOCK_METHOD2(CreateNameForAddress,
               void(const rtc::IPAddress&, NameCreatedCallback));
  MOCK_METHOD2(RemoveNameForAddress,
               void(const rtc::IPAddress&, NameRemovedCallback));
};

}  // namespace webrtc

#endif  // RTC_BASE_MOCK_MDNS_RESPONDER_H_
