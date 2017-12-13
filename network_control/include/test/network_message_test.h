/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NETWORK_CONTROL_INCLUDE_TEST_NETWORK_MESSAGE_TEST_H_
#define NETWORK_CONTROL_INCLUDE_TEST_NETWORK_MESSAGE_TEST_H_

#include <deque>
#include "network_control/include/network_message.h"
#include "test/gmock.h"

namespace webrtc {
namespace network {
namespace signal {
namespace test {

template <typename MSG_T>
class OnceReceiver : public Observer<MSG_T> {
 public:
  using handler_t = std::function<void(MSG_T)>;
  OnceReceiver() {}
  void OnMessage(MSG_T msg) override {
    RTC_DCHECK(!handlers_.empty());
    auto& handler = handlers_.front();
    handler(msg);
    handlers_.pop_front();
  }
  void AddHandler(handler_t handler) { handlers_.push_back(handler); }
  virtual ~OnceReceiver() {}

 private:
  std::deque<handler_t> handlers_;
  RTC_DISALLOW_COPY_AND_ASSIGN(OnceReceiver);
};

template <typename MSG_T>
class MockReceiver : public Observer<MSG_T> {
 public:
  MockReceiver() {}
  MOCK_METHOD1_T(OnMessage, void(MSG_T));
  virtual ~MockReceiver() {}
  RTC_DISALLOW_COPY_AND_ASSIGN(MockReceiver);
};

}  // namespace test
}  // namespace signal
}  // namespace network
}  // namespace webrtc
#endif  // NETWORK_CONTROL_INCLUDE_TEST_NETWORK_MESSAGE_TEST_H_
