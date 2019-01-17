#ifndef SIM_STUN_SERVER_H_
#define SIM_STUN_SERVER_H_

#include "p2p/base/sim_stun_server.h"

#include "p2p/base/stun.h"
#include "p2p/base/stun_server.h"

namespace webrtc {

class AsyncUDPSocket;
class SimCore;

class SimStunServer : public cricket::StunServer {
 public:
  SimStunServer(rtc::AsyncUDPSocket* socket, SimCore* core);
  ~SimStunServer() override;

 private:
  void OnBindingRequest(cricket::StunMessage* msg,
                        const rtc::SocketAddress& remote_addr) override;

  // A back-pointer to the simulation core. The STUN server is owned by the
  // core.
  SimCore* core_;
};

}  // namespace webrtc

#endif
