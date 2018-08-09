/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Usage: mdnsresponder address
//
// Where: |address| is the socket address that the mDNS responder listens for
// mDNS queries and also the source address when sending mDNS responses.
// |address| should be in the form of either "ipv4:port" for IPv4 or
// "[ipv6]:port" for IPv6.
//
// Note: The mDNS responder will send out a loopback
// response to |address| and the response contains the type-4 UUID name
// generated for |address|.

#include <memory>
#include <set>

#include "p2p/base/basic_mdns_responder.h"
#include "p2p/base/basicpacketsocketfactory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/messagehandler.h"
#include "rtc_base/thread.h"

namespace {

enum {
  MSG_DO_ANNOUNCEMENT,
};

}  // namespace

class LoopbackAnnouncementHandler : public rtc::MessageHandler,
                                    public ::sigslot::has_slots<> {
 public:
  explicit LoopbackAnnouncementHandler(webrtc::MDnsResponder* responder)
      : responder_(responder) {}
  ~LoopbackAnnouncementHandler() = default;

  void OnMessage(rtc::Message* msg) override {
    RTC_DCHECK(msg->message_id == MSG_DO_ANNOUNCEMENT);
    DoAnnouncement();
  }

  void set_name(const std::string& name) { name_ = name; }
  void set_loopback_address(const rtc::SocketAddress& addr) {
    loopback_address_ = addr;
  }

  void OnAnnouncementSent() {
    RTC_LOG(INFO) << "Sent announcment for name " << name_;
  }

 private:
  void DoAnnouncement() {
    responder_->OnQueryReceived(1, loopback_address_, {name_}, true);
  }

  webrtc::MDnsResponder* responder_;
  std::string name_;
  rtc::SocketAddress loopback_address_;
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    RTC_LOG(LS_ERROR) << "usage: mdnsresponder address";
    return 1;
  }

  rtc::SocketAddress addr;
  if (!addr.FromString(argv[1])) {
    RTC_LOG(LS_ERROR) << "Unable to parse IP address: ";
    return 1;
  }

  rtc::Thread* main_thread = rtc::Thread::Current();
  rtc::BasicPacketSocketFactory socket_factory(main_thread);

  std::unique_ptr<rtc::AsyncPacketSocket> socket(
      socket_factory.CreateUdpSocket(addr, addr.port(), addr.port()));
  if (!socket || socket->GetState() != rtc::AsyncPacketSocket::STATE_BOUND) {
    RTC_LOG(LS_ERROR) << "Failed to create and binding a socket @ "
                      << addr.ToString();
    return 1;
  }

  bool addr_is_ipv4 = (addr.family() == AF_INET);
  std::unique_ptr<webrtc::BasicMDnsResponder> responder(
      new webrtc::BasicMDnsResponder(addr_is_ipv4 ? socket.get() : nullptr,
                                     addr_is_ipv4 ? nullptr : socket.get(),
                                     socket.get()));
  LoopbackAnnouncementHandler handler(responder.get());
  responder->SignalResponseSent.connect(
      &handler, &LoopbackAnnouncementHandler::OnAnnouncementSent);
  RTC_LOG(INFO) << "Creating name for address "
                << socket->GetLocalAddress().ipaddr().ToString();
  const std::string name =
      responder->CreateNameForAddress(socket->GetLocalAddress().ipaddr());
  RTC_LOG(INFO) << "Name created: " << name;
  handler.set_name(name);
  handler.set_loopback_address(socket->GetLocalAddress());

  RTC_LOG(INFO) << "Starting mDNS responder";

  main_thread->Post(RTC_FROM_HERE, &handler, MSG_DO_ANNOUNCEMENT);
  main_thread->Run();

  return 0;
}
