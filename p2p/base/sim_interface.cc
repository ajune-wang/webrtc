#include "p2p/base/sim_interface.h"

#include "p2p/base/sim_core.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

SimPlatformSocket::SimPlatformSocket() = default;

SimPlatformSocket::SimPlatformSocket(int sockfd, const rtc::SocketAddress& addr)
    : sockfd(sockfd), addr(addr) {}

SimPlatformSocket::~SimPlatformSocket() = default;

SimInterface::SimInterface(int fd,
                           const std::string& name,
                           const rtc::IPAddress& ip,
                           int prefix_length,
                           rtc::AdapterType type,
                           std::unique_ptr<SimInterface> dual_iface,
                           SimCore* core)
    : fd_(fd),
      name_(name),
      ip_(ip),
      ip_str_(ip_.ToString()),
      prefix_length_(prefix_length),
      type_(type),
      dual_iface_(std::move(dual_iface)),
      core_(core),
      weak_factory_(this) {
  role_ = (dual_iface == nullptr ? Role::kPrime : Role::kDual);
}

SimInterface::~SimInterface() {
  close(fd_);
}

rtc::Network* SimInterface::ToRtcNetwork() {
  if (network_ != nullptr) {
    return network_.get();
  }
  rtc::IPAddress prefix =
      TruncateIP(rtc::SocketAddress(ip_, 0).ipaddr(), prefix_length_);
  network_ = absl::make_unique<rtc::Network>(name_, name_, prefix,
                                             prefix_length_, type_);
  network_->AddIP(rtc::SocketAddress(ip_, 0).ipaddr());
  return network_.get();
}

void SimInterface::SetState(SimInterface::State state) {
  state_ = state;
  SignalStateChanged(this);
}

rtc::WeakPtr<SimInterface> SimInterface::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SimInterface::OnPacketReceived(rtc::scoped_refptr<SimPacket> packet,
                                    const rtc::SocketAddress& src_addr,
                                    int dst_port) {
  RTC_DCHECK_RUN_ON(core_->nio_thread());
  SignalPacketReceived(packet, src_addr, this, dst_port);
}

}  // namespace webrtc
