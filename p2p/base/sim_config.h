#ifndef SIMCONFIG_H_
#define SIMCONFIG_H_

#include <string>
#include <vector>

#include "p2p/base/sim_interface.h"
#include "p2p/base/sim_link.h"

#include "absl/types/optional.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"

namespace webrtc {

struct SimInterfaceConfig {
  SimInterfaceConfig(const std::string& name,
                     const std::string& ip,
                     const std ::string& mask,
                     rtc::AdapterType type,
                     SimInterface::State init_state);
  explicit SimInterfaceConfig(const SimInterfaceConfig& other);
  ~SimInterfaceConfig();

  std::string name;
  std::string ip;
  std::string mask;
  rtc::AdapterType type;
  SimInterface::State init_state;
};

struct SimLinkConfig {
  struct Params {
    Params();
    explicit Params(const Params& other);
    ~Params();
    absl::optional<uint32_t> bw_bps;
    absl::optional<double> drop_prob;
  };

  SimLinkConfig(const std::string& name,
                SimLink::Type type,
                const std::vector<std::string>& iface_ips,
                const Params& params);
  explicit SimLinkConfig(const SimLinkConfig& other);
  ~SimLinkConfig();

  std::string name;
  SimLink::Type type;
  std::vector<std::string> iface_ips;
  Params params;
};

struct SimConfig {
 public:
  SimConfig();
  explicit SimConfig(const SimConfig& other);
  ~SimConfig();

  bool IsValid() const;

  rtc::Thread* webrtc_network_thread;
  std::vector<SimInterfaceConfig> iface_configs;
  std::vector<SimLinkConfig> link_configs;
};

}  // namespace webrtc

#endif  // SIMCONFIG_H_
