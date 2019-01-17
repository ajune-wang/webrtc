#include "p2p/base/sim_stun_server.h"

#include "p2p/base/sim_core.h"

#include "rtc_base/logging.h"

namespace webrtc {

SimStunServer::SimStunServer(rtc::AsyncUDPSocket* socket, SimCore* core)
    : StunServer(socket), core_(core) {}

SimStunServer::~SimStunServer() = default;

void SimStunServer::OnBindingRequest(cricket::StunMessage* msg,
                                     const rtc::SocketAddress& remote_addr) {
  RTC_LOG(INFO) << "Received STUN binding request from "
                << remote_addr.ToString();
  SimInterface* iface = core_->GetInterface(remote_addr.ipaddr());
  if (iface == nullptr) {
    RTC_LOG(LS_ERROR)
        << "Received STUN binding request from an unknown interface.";
  }
  cricket::StunMessage response;
  const rtc::SocketAddress mapped_address(iface->dual()->ip(),
                                          remote_addr.port());
  RTC_LOG(INFO) << "mapped_address = " << mapped_address.ToString();
  GetStunBindReqponse(msg, mapped_address, &response);
  SendResponse(response, remote_addr);
}

}  // namespace webrtc
