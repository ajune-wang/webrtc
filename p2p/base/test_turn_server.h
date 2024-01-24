/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TEST_TURN_SERVER_H_
#define P2P_BASE_TEST_TURN_SERVER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/sequence_checker.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/turn_server.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/thread.h"

namespace cricket {

class TestTurnRedirector : public TurnRedirectInterface {
 public:
  explicit TestTurnRedirector(const std::vector<rtc::SocketAddress>& addresses);

  bool ShouldRedirect(const rtc::SocketAddress&,
                      rtc::SocketAddress* out) override;

 private:
  const std::vector<rtc::SocketAddress>& alternate_server_addresses_;
  std::vector<rtc::SocketAddress>::const_iterator iter_;
};

class TestTurnServer : public TurnAuthInterface {
 public:
  TestTurnServer(rtc::Thread* thread,
                 rtc::SocketFactory* socket_factory,
                 const rtc::SocketAddress& int_addr,
                 const rtc::SocketAddress& udp_ext_addr,
                 ProtocolType int_protocol = PROTO_UDP,
                 bool ignore_bad_cert = true,
                 absl::string_view common_name = "test turn server");

  ~TestTurnServer() override;

  void set_enable_otu_nonce(bool enable) {
    RTC_DCHECK(thread_checker_.IsCurrent());
    server_.set_enable_otu_nonce(enable);
  }

  TurnServer* server() {
    RTC_DCHECK(thread_checker_.IsCurrent());
    return &server_;
  }

  void set_redirect_hook(TurnRedirectInterface* redirect_hook) {
    RTC_DCHECK(thread_checker_.IsCurrent());
    server_.set_redirect_hook(redirect_hook);
  }

  void set_enable_permission_checks(bool enable) {
    RTC_DCHECK(thread_checker_.IsCurrent());
    server_.set_enable_permission_checks(enable);
  }

  void AddInternalSocket(const rtc::SocketAddress& int_addr,
                         ProtocolType proto,
                         bool ignore_bad_cert = true,
                         absl::string_view common_name = "test turn server");

  // Finds the first allocation in the server allocation map with a source
  // ip and port matching the socket address provided.
  TurnServerAllocation* FindAllocation(const rtc::SocketAddress& src);

 private:
  // For this test server, succeed if the password is the same as the username.
  // Obviously, do not use this in a production environment.
  bool GetKey(absl::string_view username,
              absl::string_view realm,
              std::string* key) override;

  TurnServer server_;
  rtc::SocketFactory* socket_factory_;
  webrtc::SequenceChecker thread_checker_;
};

}  // namespace cricket

#endif  // P2P_BASE_TEST_TURN_SERVER_H_
