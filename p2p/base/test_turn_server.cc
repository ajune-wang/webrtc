/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/test_turn_server.h"

#include "api/transport/stun.h"
#include "p2p/base/basic_packet_socket_factory.h"

namespace cricket {
namespace {
constexpr char kTestRealm[] = "example.org";
constexpr char kTestSoftware[] = "TestTurnServer";
}  // namespace

TestTurnRedirector::TestTurnRedirector(
    const std::vector<rtc::SocketAddress>& addresses)
    : alternate_server_addresses_(addresses),
      iter_(alternate_server_addresses_.begin()) {}

bool TestTurnRedirector::ShouldRedirect(const rtc::SocketAddress&,
                                        rtc::SocketAddress* out) {
  if (!out || iter_ == alternate_server_addresses_.end()) {
    return false;
  }
  *out = *iter_++;
  return true;
}

TestTurnServer::TestTurnServer(rtc::Thread* thread,
                               rtc::SocketFactory* socket_factory,
                               const rtc::SocketAddress& int_addr,
                               const rtc::SocketAddress& udp_ext_addr,
                               ProtocolType int_protocol,
                               bool ignore_bad_cert,
                               absl::string_view common_name)
    : server_(thread), socket_factory_(socket_factory) {
  AddInternalSocket(int_addr, int_protocol, ignore_bad_cert, common_name);
  server_.SetExternalSocketFactory(
      new rtc::BasicPacketSocketFactory(socket_factory), udp_ext_addr);
  server_.set_realm(kTestRealm);
  server_.set_software(kTestSoftware);
  server_.set_auth_hook(this);
}

TestTurnServer::~TestTurnServer() {
  RTC_DCHECK(thread_checker_.IsCurrent());
}

void TestTurnServer::AddInternalSocket(const rtc::SocketAddress& int_addr,
                                       ProtocolType proto,
                                       bool ignore_bad_cert,
                                       absl::string_view common_name) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (proto == cricket::PROTO_UDP) {
    server_.AddInternalSocket(
        rtc::AsyncUDPSocket::Create(socket_factory_, int_addr), proto);
  } else if (proto == cricket::PROTO_TCP || proto == cricket::PROTO_TLS) {
    // For TCP we need to create a server socket which can listen for incoming
    // new connections.
    rtc::Socket* socket = socket_factory_->CreateSocket(AF_INET, SOCK_STREAM);
    socket->Bind(int_addr);
    socket->Listen(5);
    if (proto == cricket::PROTO_TLS) {
      // For TLS, wrap the TCP socket with an SSL adapter. The adapter must
      // be configured with a self-signed certificate for testing.
      // Additionally, the client will not present a valid certificate, so we
      // must not fail when checking the peer's identity.
      std::unique_ptr<rtc::SSLAdapterFactory> ssl_adapter_factory =
          rtc::SSLAdapterFactory::Create();
      ssl_adapter_factory->SetRole(rtc::SSL_SERVER);
      ssl_adapter_factory->SetIdentity(
          rtc::SSLIdentity::Create(common_name, rtc::KeyParams()));
      ssl_adapter_factory->SetIgnoreBadCert(ignore_bad_cert);
      server_.AddInternalServerSocket(socket, proto,
                                      std::move(ssl_adapter_factory));
    } else {
      server_.AddInternalServerSocket(socket, proto);
    }
  } else {
    RTC_DCHECK_NOTREACHED() << "Unknown protocol type: " << proto;
  }
}

TurnServerAllocation* TestTurnServer::FindAllocation(
    const rtc::SocketAddress& src) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  const TurnServer::AllocationMap& map = server_.allocations();
  for (TurnServer::AllocationMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    if (src == it->first.src()) {
      return it->second.get();
    }
  }
  return NULL;
}

bool TestTurnServer::GetKey(absl::string_view username,
                            absl::string_view realm,
                            std::string* key) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  return ComputeStunCredentialHash(std::string(username), std::string(realm),
                                   std::string(username), key);
}

}  // namespace cricket
