#include "p2p/base/sim_config.h"

#include "rtc_base/ipaddress.h"
#include "rtc_base/logging.h"

namespace webrtc {

SimInterfaceConfig::SimInterfaceConfig(const std::string& name,
                                       const std::string& ip,
                                       const std ::string& mask,
                                       rtc::AdapterType type,
                                       SimInterface::State init_state)
    : name(name), ip(ip), mask(mask), type(type), init_state(init_state) {}

SimInterfaceConfig::SimInterfaceConfig(const SimInterfaceConfig& other) =
    default;

SimInterfaceConfig::~SimInterfaceConfig() = default;

SimLinkConfig::Params::Params() = default;

SimLinkConfig::Params::Params(const SimLinkConfig::Params& other) = default;

SimLinkConfig::Params::~Params() = default;

SimLinkConfig::SimLinkConfig(const std::string& name,
                             SimLink::Type type,
                             const std::vector<std::string>& iface_ips,
                             const Params& params)
    : name(name), type(type), iface_ips(iface_ips), params(params) {}

SimLinkConfig::SimLinkConfig(const SimLinkConfig& other) = default;

SimLinkConfig::~SimLinkConfig() = default;

SimConfig::SimConfig() = default;

SimConfig::SimConfig(const SimConfig& other) = default;

SimConfig::~SimConfig() = default;

bool SimConfig::IsValid() const {
  if (webrtc_network_thread == nullptr) {
    return false;
  }
  rtc::IPAddress out;
  std::set<std::string> iface_ips;
  for (const auto& cfg : iface_configs) {
    if (!rtc::IPFromString(cfg.ip, &out) ||
        !rtc::IPFromString(cfg.mask, &out)) {
      RTC_LOG(LS_ERROR) << "Interface configured with an invalid address";
      return false;
    }
    iface_ips.insert(cfg.ip);
  }

  for (const auto& cfg : link_configs) {
    for (const std::string& ip : cfg.iface_ips) {
      if (!rtc::IPFromString(ip, &out)) {
        RTC_LOG(LS_ERROR) << "Link configured with an invalid interface";
        return false;
      }
      if (iface_ips.find(ip) == iface_ips.end()) {
        RTC_LOG(LS_ERROR) << "Link configured with a non-existing interface";
        return false;
      }
    }
  }
  return true;
}

}  // namespace webrtc
