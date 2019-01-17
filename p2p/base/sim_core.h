#ifndef SIM_CORE_H_
#define SIM_CORE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "p2p/base/sim_config.h"
#include "p2p/base/sim_interface.h"
#include "p2p/base/sim_link.h"
#include "p2p/base/sim_packet.h"
#include "p2p/base/sim_stun_server.h"

#include "api/candidate.h"
#include "api/scoped_refptr.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/buffer.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace webrtc {

class SimCore;

class SimNetworkManager : public rtc::NetworkManager,
                          public SimInterfaceObserver {
 public:
  // TODO(qingsi): Weak pointer to the core.
  SimNetworkManager(rtc::Thread* webrtc_network_thread, SimCore* core);
  ~SimNetworkManager() override;

  // NetworkManager overrides.
  void StartUpdating() override;
  void StopUpdating() override;
  void GetNetworks(NetworkList* networks) const override;

  // SimInterfaceObserver overrides.
  void OnInterfaceStateChanged(SimInterface* iface) override;

  void AddInterface(rtc::WeakPtr<SimInterface> iface);

  SimCore* core() { return core_; }

 private:
  rtc::Thread* webrtc_network_thread_;
  std::vector<rtc::WeakPtr<SimInterface>> ifaces_;
  SimCore* core_;
};

class SimCore : public ::sigslot::has_slots<> {
 public:
  SimCore();
  ~SimCore() override;
  bool Init(const SimConfig& config);
  void Start();
  // Stopping the simulation core releases all resources of virtual network
  // interfaces, and to restart the simluation, the core must be re-initialized
  // with a config via Init() and then Start().
  void Stop();
  void OnPacketReceived(rtc::scoped_refptr<SimPacket> packet,
                        const rtc::SocketAddress& src_addr,
                        SimInterface* dst_iface,
                        int dst_port);
  void OnInterfaceError(SimInterface* iface);

  bool started() const { return started_; }
  rtc::Thread* main_thread() { return main_thread_; }
  rtc::Thread* nio_thread() { return nio_thread_.get(); }

  std::unique_ptr<SimNetworkManager> CreateNetworkManager(
      const std::set<std::string>& iface_names);

  SimInterface* GetInterface(const std::string& iface_name) const;
  SimInterface* GetInterface(const rtc::IPAddress& ip) const;

  bool CreateAndBindSocketOnDualInterface(SimInterface* dual_iface,
                                          int port_to_bind);

 private:
  std::unique_ptr<SimInterface> CreateInterface(
      const SimInterfaceConfig& config,
      bool is_dual);
  std::unique_ptr<SimLink> CreateLink(const SimLinkConfig& config);
  std::unique_ptr<SimLink> CreatePointToPointLink(const SimLinkConfig& config);
  bool CreateStunServer();

  void ReadAndBufferPacket(SimInterface* iface, int sockfd);

  void ReplayPacket(rtc::scoped_refptr<SimPacket> packet,
                    SimInterface* src_iface,
                    int src_port,
                    SimInterface* dst_iface,
                    int dst_port);

  rtc::CriticalSection crit_;
  bool started_ = false;
  rtc::BufferT<uint8_t> buffer_;
  int pipefd_[2];
  rtc::Thread* main_thread_;
  std::unique_ptr<rtc::Thread> nio_thread_;
  rtc::Thread* webrtc_network_thread_;
  std::vector<std::unique_ptr<SimInterface>> ifaces_;
  std::vector<std::unique_ptr<SimInterface>> dual_ifaces_;
  std::vector<std::unique_ptr<SimLink>> links_;
  std::unique_ptr<SimStunServer> stun_server_;
  std::map<int, SimPlatformSocket> dual_socket_by_fd_;
  std::map<std::string, SimPlatformSocket> dual_socket_by_addr_;
  // Technically we could have two interfaces on different subnet with the same
  // IP (hence different prefix in this case). For the simulation, we rule out
  // such network configuration, and assume the interfaces have unique
  // addresses.
  std::map<std::string, SimInterface*> iface_by_ip_;
  std::map<SimInterface*, int> total_packets_recv_by_iface_;
};

}  // namespace webrtc

#endif
