#ifndef SIM_INTERFACE_H_
#define SIM_INTERFACE_H_

#include <linux/if.h>
#include <sys/socket.h>

#include <memory>
#include <string>
#include <vector>

#include "p2p/base/sim_packet.h"

#include "rtc_base/ipaddress.h"
#include "rtc_base/network.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class SimCore;

struct SimPlatformSocket {
  SimPlatformSocket();
  SimPlatformSocket(int sockfd, const rtc::SocketAddress& addr);
  ~SimPlatformSocket();
  int sockfd;
  rtc::SocketAddress addr;
};

class SimInterface {
 public:
  enum class State {
    kUp,
    kDown,
  };

  enum class Role {
    kUnknown,
    kPrime,
    kDual,
  };

  SimInterface(int fd,
               const std::string& name,
               const rtc::IPAddress& ip,
               int prefix_length,
               rtc::AdapterType type,
               std::unique_ptr<SimInterface> dual_iface,
               SimCore* core);
  ~SimInterface();

  int fd() const { return fd_; }
  void AddDualSocket(const SimPlatformSocket& socket) {
    RTC_DCHECK(role_ == Role::kDual);
    dual_sockets_.push_back(socket);
  }
  rtc::Network* ToRtcNetwork();
  const std::string& name() const { return name_; }
  const rtc::IPAddress& ip() const { return ip_; }
  const std::string& ip_str() const { return ip_str_; }
  SimInterface* dual() {
    // Only the prime interface has a dual.
    RTC_DCHECK(role_ == Role::kPrime);
    return dual_iface_.get();
  }
  const std::vector<SimPlatformSocket>& dual_sockets() const {
    if (role_ == Role::kPrime) {
      return dual_iface_->dual_sockets_;
    }

    return dual_sockets_;
  }
  State state() { return state_; }
  Role role() { return role_; }
  void set_role(Role role) { role_ = role; }
  void SetState(State state);

  rtc::WeakPtr<SimInterface> GetWeakPtr();

  void OnPacketReceived(rtc::scoped_refptr<SimPacket> packet,
                        const rtc::SocketAddress& src_addr,
                        int dst_port);

  sigslot::signal4<rtc::scoped_refptr<SimPacket>,
                   const rtc::SocketAddress&,
                   SimInterface*,
                   int>
      SignalPacketReceived;
  sigslot::signal1<SimInterface*> SignalStateChanged;

 private:
  int fd_;
  std::string name_;
  rtc::IPAddress ip_;
  std::string ip_str_;
  int prefix_length_;
  rtc::AdapterType type_;
  std::unique_ptr<rtc::Network> network_;
  State state_ = State::kDown;
  Role role_ = Role::kUnknown;
  std::unique_ptr<SimInterface> dual_iface_;
  std::vector<SimPlatformSocket> dual_sockets_;
  // A back-pointer to the simulation core. The simulation core should outlive
  // the interface.
  SimCore* core_;
  rtc::WeakPtrFactory<SimInterface> weak_factory_;
};

class SimInterfaceObserver : public ::sigslot::has_slots<> {
 public:
  ~SimInterfaceObserver() override = default;
  virtual void OnInterfaceStateChanged(SimInterface* iface) = 0;
};

}  // namespace webrtc

#endif  // SIM_INTERFACE_H_
