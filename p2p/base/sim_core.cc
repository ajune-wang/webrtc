#include "p2p/base/sim_core.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/byteorder.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timeutils.h"

namespace {

const char* kCloneDev = "/dev/net/tun";
const int kMaxIpPacketSize = 0xffff;
const int kIpv6AddressSizeBytes = 16;

}  // namespace

namespace webrtc {

SimNetworkManager::SimNetworkManager(rtc::Thread* webrtc_network_thread,
                                     SimCore* core)
    : webrtc_network_thread_(webrtc_network_thread), core_(core) {}

SimNetworkManager::~SimNetworkManager() {
  disconnect_all();
  // This is thread safe since it is synchronous.
  webrtc_network_thread_->Invoke<void>(RTC_FROM_HERE,
                                       [this]() { ifaces_.clear(); });
}

void SimNetworkManager::StartUpdating() {
  RTC_DCHECK_RUN_ON(webrtc_network_thread_);
  SignalNetworksChanged();
}

void SimNetworkManager::StopUpdating() {
  RTC_DCHECK_RUN_ON(webrtc_network_thread_);
}

void SimNetworkManager::GetNetworks(NetworkList* networks) const {
  RTC_DCHECK_RUN_ON(webrtc_network_thread_);
  for (auto& iface : ifaces_) {
    if (iface != nullptr && iface->state() == SimInterface::State::kUp) {
      networks->push_back(iface->ToRtcNetwork());
    }
  }
}

void SimNetworkManager::OnInterfaceStateChanged(SimInterface* iface) {
  // This is thread safe since it is synchronous.
  webrtc_network_thread_->Invoke<void>(RTC_FROM_HERE,
                                       [this]() { SignalNetworksChanged(); });
}

void SimNetworkManager::AddInterface(rtc::WeakPtr<SimInterface> iface) {
  ifaces_.emplace_back(std::move(iface));
  SignalNetworksChanged();
}

SimCore::SimCore()
    : buffer_(kMaxIpPacketSize),
      main_thread_(rtc::Thread::Current()),
      nio_thread_(rtc::Thread::Create()) {
  nio_thread_->SetName("simcore_nio_thread", nullptr);
  nio_thread_->Start();
}

SimCore::~SimCore() {
  if (started_) {
    Stop();
  }
  close(pipefd_[0]);
  close(pipefd_[1]);
}

bool SimCore::Init(const SimConfig& config) {
  // TODO(qingsi): check the threading.
  if (!config.IsValid()) {
    RTC_LOG(LS_ERROR) << "Configuration is invalid.";
    return false;
  }
  if (pipe(pipefd_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to create the signaling pipe.";
    return false;
  }
  for (const auto& iface_config : config.iface_configs) {
    auto iface = CreateInterface(iface_config, false /* is_dual */);
    if (iface == nullptr) {
      RTC_LOG(LS_ERROR) << "Failed to create simulation iface";
      return false;
    }
    const std::string ip = iface->ip_str();
    iface->SignalPacketReceived.connect(this, &SimCore::OnPacketReceived);
    RTC_DCHECK(iface_by_ip_.find(ip) == iface_by_ip_.end());
    iface_by_ip_[ip] = iface.get();
    ifaces_.emplace_back(std::move(iface));
  }

  for (const auto& link_config : config.link_configs) {
    auto link = CreateLink(link_config);
    link->SignalPacketReadyToReplay.connect(this, &SimCore::ReplayPacket);
    links_.emplace_back(std::move(link));
  }

  RTC_LOG(INFO) << "before stun server";
  if (!CreateStunServer()) {
    RTC_LOG(INFO) << "Failed to create simulation STUN server.";
  }
  RTC_LOG(INFO) << "after stun server";

  webrtc_network_thread_ = config.webrtc_network_thread;
  return true;
}

void SimCore::Start() {
  RTC_LOG(INFO) << "Starting simulation core.";
  RTC_DCHECK_RUN_ON(nio_thread());
  fd_set fds_read;
  FD_ZERO(&fds_read);
  fd_set fds_write;
  FD_ZERO(&fds_write);
  started_ = true;
  while (started_) {
    int fd_max = -1;
    {
      rtc::CritScope cr(&crit_);
      FD_SET(pipefd_[1], &fds_write);
      fd_max = pipefd_[0] > pipefd_[1] ? pipefd_[0] : pipefd_[1];
      for (const auto& iface : ifaces_) {
        for (const auto& sock : iface->dual_sockets()) {
          FD_SET(sock.sockfd, &fds_read);
          if (sock.sockfd > fd_max) {
            fd_max = sock.sockfd;
          }
        }
      }
    }

    // Wait forever.
    int rv = select(fd_max + 1, &fds_read, &fds_write,
                    nullptr /* fds exception*/, nullptr /* timeout */);
    if (rv < 0) {
      if (errno != EINTR) {
        // TODO(qingsi): There is a bug. Need to stop the select when destroyed.
        RTC_LOG(LS_ERROR) << "Error in select().";
        started_ = false;
        return;
      }
    } else if (rv == 0) {
      RTC_NOTREACHED();
      started_ = false;
      return;
    } else {
      rtc::CritScope cr(&crit_);
      if (FD_ISSET(pipefd_[0], &fds_read)) {
        RTC_LOG(INFO)
            << "Received signal to stop the simulation core from pipe 0.";
        started_ = false;
        return;
      }
      if (FD_ISSET(pipefd_[1], &fds_write)) {
        if (!started_) {
          RTC_LOG(INFO)
              << "Received signal to stop the simulation core from pipe 1.";
          return;
        }
        FD_CLR(pipefd_[1], &fds_write);
      }
      for (const auto& iface : ifaces_) {
        for (const auto& sock : iface->dual_sockets()) {
          bool readable = FD_ISSET(sock.sockfd, &fds_read);
          if (readable) {
            RTC_LOG(INFO) << "Readable";
            FD_CLR(sock.sockfd, &fds_read);
            ReadAndBufferPacket(iface.get(), sock.sockfd);
          }
        }
      }
    }
  }
}

void SimCore::Stop() {
  started_ = false;
  const uint8_t b[1] = {0};
  int rv = write(pipefd_[1], b, sizeof(b));
  RTC_DCHECK_EQ(1, rv);
  // TODO(qingsi): Need to gracefully shut down nio_thread_.
  // webrtc_network_thread_ must outlive the core.
  webrtc_network_thread_->Invoke<void>(RTC_FROM_HERE, [this]() {
    ifaces_.clear();
    iface_by_ip_.clear();
  });
}

void SimCore::OnPacketReceived(rtc::scoped_refptr<SimPacket> packet,
                               const rtc::SocketAddress& src_addr,
                               SimInterface* dst_iface,
                               int dst_port) {
  RTC_DCHECK_RUN_ON(nio_thread());
  ++total_packets_recv_by_iface_[dst_iface];
}

void SimCore::OnInterfaceError(SimInterface* iface) {
  RTC_DCHECK_RUN_ON(nio_thread());
  RTC_DCHECK(iface != nullptr);
  iface->SetState(SimInterface::State::kDown);
}

std::unique_ptr<SimNetworkManager> SimCore::CreateNetworkManager(
    const std::set<std::string>& iface_names) {
  RTC_DCHECK(webrtc_network_thread_ != nullptr);
  auto network_manager =
      absl::make_unique<SimNetworkManager>(webrtc_network_thread_, this);
  for (const auto& iface : ifaces_) {
    if (iface_names.find(iface->name()) != iface_names.end()) {
      // All interfaces added here should only be used on webrtc_network_thread;
      network_manager->AddInterface(iface->GetWeakPtr());
      iface->SignalStateChanged.connect(
          network_manager.get(), &SimNetworkManager::OnInterfaceStateChanged);
    }
  }
  return network_manager;
}

SimInterface* SimCore::GetInterface(const std::string& iface_name) const {
  for (const auto& iface : ifaces_) {
    if (iface->name() == iface_name) {
      return iface.get();
    }
  }
  return nullptr;
}

SimInterface* SimCore::GetInterface(const rtc::IPAddress& ip) const {
  const std::string ip_str(ip.ToString());
  const auto it = iface_by_ip_.find(ip_str);
  if (it == iface_by_ip_.end()) {
    return nullptr;
  }
  return it->second;
}

std::unique_ptr<SimInterface> SimCore::CreateInterface(
    const SimInterfaceConfig& config,
    bool is_dual) {
  const std::string& iface_name = config.name;
  // Open the clone device.
  int fd = open(kCloneDev, O_RDWR);
  if (fd < 0) {
    RTC_LOG(LS_ERROR) << "Cannot open the clone device";
    return nullptr;
  }

  // Set the descriptor to be non-blocking.
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  strncpy(ifr.ifr_name, iface_name.c_str(), IFNAMSIZ);
  if (ioctl(fd, TUNSETIFF, static_cast<void*>(&ifr)) < 0) {
    RTC_LOG(LS_ERROR) << "Cannot ioctl the tun interface " << iface_name
                      << ", err=" << errno;
    close(fd);
    return nullptr;
  }
#if 1
  if (ioctl(fd, TUNSETPERSIST, 1)) {
    RTC_LOG(LS_ERROR) << "Cannot make interface " << iface_name
                      << " persistent";
  }
#endif
  int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  inet_pton(AF_INET, config.ip.c_str(), &addr.sin_addr.s_addr);
  addr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
  if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
    RTC_LOG(LS_ERROR) << "Cannot assign IP address to interface " << iface_name
                      << ", err=" << errno;
    close(fd);
    return nullptr;
  }

  inet_pton(AF_INET, config.mask.c_str(), &addr.sin_addr.s_addr);
  memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
  if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
    RTC_LOG(LS_ERROR) << "Cannot assign IP mask to interface " << iface_name
                      << ", err=" << errno;
    close(fd);
    return nullptr;
  }
  ifr.ifr_flags |= IFF_UP | IFF_RUNNING | IFF_MULTICAST | IFF_NOARP;
  if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
    RTC_LOG(LS_ERROR) << "Cannot set flags for interface " << iface_name
                      << ", err=" << errno;
    close(fd);
    return nullptr;
  }
  const int prefix_length =
      rtc::CountIPMaskBits(rtc::SocketAddress(config.mask, 0).ipaddr());

  rtc::IPAddress ip(rtc::SocketAddress(config.ip, 0).ipaddr());
  if (!is_dual) {
    // TODO(qingsi): make the following a method.
    rtc::IPAddress dual_ip;
    if (ip.family() == AF_INET) {
      in_addr raw_ip = ip.ipv4_address();
      uint32_t last_octet = raw_ip.s_addr & 0xff000000;
      in_addr dual_raw_ip;
      dual_raw_ip.s_addr =
          (raw_ip.s_addr & 0x00ffffff) | (0xff000000 - last_octet);
      dual_ip = rtc::IPAddress(dual_raw_ip);
    } else {
      in6_addr raw_ip = ip.ipv6_address();
      uint8_t last_octet = raw_ip.s6_addr[kIpv6AddressSizeBytes - 1];
      in6_addr dual_raw_ip;
      memcpy(&dual_raw_ip, &raw_ip, kIpv6AddressSizeBytes);
      dual_raw_ip.s6_addr[kIpv6AddressSizeBytes - 1] =
          raw_ip.s6_addr[kIpv6AddressSizeBytes - 1] - last_octet;
      dual_ip = rtc::IPAddress(dual_raw_ip);
    }

    SimInterfaceConfig dual_config(config);
    dual_config.name = config.name + "_dual";
    dual_config.ip = dual_ip.ToString();
    auto dual_iface = CreateInterface(dual_config, true /* is_dual */);
    auto iface = absl::make_unique<SimInterface>(fd, config.name, ip,
                                                 prefix_length, config.type,
                                                 std::move(dual_iface), this);
    iface->SetState(config.init_state);
    return iface;
  }
  auto iface = absl::make_unique<SimInterface>(
      fd, config.name, ip, prefix_length, config.type, nullptr, this);
  iface->set_role(SimInterface::Role::kDual);
  return iface;
}

std::unique_ptr<SimLink> SimCore::CreateLink(const SimLinkConfig& config) {
  switch (config.type) {
    case SimLink::Type::kPointToPoint: {
      return CreatePointToPointLink(config);
    }
    default:
      RTC_LOG(LS_ERROR) << "Not implemented.";
      RTC_NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<SimLink> SimCore::CreatePointToPointLink(
    const SimLinkConfig& config) {
  RTC_LOG(INFO) << "SimCore::CreatePointToPointLink";
  RTC_DCHECK_EQ(2u, config.iface_ips.size());
  RTC_DCHECK(iface_by_ip_.find(config.iface_ips[0]) != iface_by_ip_.end());
  RTC_DCHECK(iface_by_ip_.find(config.iface_ips[1]) != iface_by_ip_.end());
  auto* iface1 = iface_by_ip_[config.iface_ips[0]];
  auto* iface2 = iface_by_ip_[config.iface_ips[1]];
  BasicPointToPointLink::Builder builder(nio_thread(), iface1, iface2);
  auto link = builder.SetBandwidth(config.params.bw_bps)
                  .SetPacketDropProbability(config.params.drop_prob)
                  .Build();
  iface1->SignalPacketReceived.connect(link.get(), &SimLink::OnPacketReceived);
  iface2->SignalPacketReceived.connect(link.get(), &SimLink::OnPacketReceived);
  return link;
}

bool SimCore::CreateStunServer() {
  const webrtc::SimInterfaceConfig config{
      "tun_stun", "99.99.99.1", "255.255.255.0", rtc::ADAPTER_TYPE_WIFI,
      webrtc::SimInterface::State::kUp};
//  auto stun_server_iface = CreateInterface(config, false);
//  if (stun_server_iface == nullptr) {
//    RTC_LOG(LS_ERROR) << "Failed to create simulation iface";
//    return false;
//  }
//  std::string ip = stun_server_iface->ip_str();
//  RTC_DCHECK(iface_by_ip_.find(ip) == iface_by_ip_.end());
//  iface_by_ip_[ip] = stun_server_iface.get();
  rtc::AsyncSocket* socket =
      rtc::Thread::Current()->socketserver()->CreateAsyncSocket(
          AF_INET, SOCK_DGRAM);
  rtc::AsyncUDPSocket* udp_socket = rtc::AsyncUDPSocket::Create(
      socket, rtc::SocketAddress("99.99.99.254",
                                 cricket::STUN_SERVER_PORT));
  stun_server_ = absl::make_unique<SimStunServer>(udp_socket, this);
//  ifaces_.emplace_back(std::move(stun_server_iface));
  return true;
}

void SimCore::ReadAndBufferPacket(SimInterface* iface, int sockfd) {
  // Whenever we received anything, the packet should be sent from a prime
  // interface to a dual interface.
  RTC_DCHECK_RUN_ON(nio_thread());
  RTC_DCHECK(iface->role() == SimInterface::Role::kPrime);
  const auto& it = dual_socket_by_fd_.find(sockfd);
  RTC_DCHECK(it != dual_socket_by_fd_.end());
  RTC_DCHECK(it->second.sockfd == sockfd);
  const int dst_port = it->second.addr.port();
  sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  int rv = ::recvfrom(sockfd, reinterpret_cast<char*>(buffer_.data()),
                      static_cast<int>(buffer_.size()), 0, addr, &addr_len);
  if (rv < 0 && rv != EWOULDBLOCK) {
    RTC_LOG(LS_ERROR) << "Cannot read from interface " << iface->name();
    OnInterfaceError(iface);
    return;
  }
  RTC_LOG(LS_VERBOSE) << "Read " << rv << " bytes from interface "
                      << iface->name();

  rtc::SocketAddress remote_addr;
  if (rtc::SocketAddressFromSockAddrStorage(addr_storage, &remote_addr)) {
    RTC_LOG(LS_VERBOSE) << "Received packet from " << remote_addr.ToString();
  } else {
    RTC_LOG(LS_ERROR) << "Cannot parse the source address of the packet.";
    return;
  }

  const auto& it1 = iface_by_ip_.find(remote_addr.ipaddr().ToString());
  if (it1 == iface_by_ip_.end()) {
    RTC_LOG(LS_ERROR) << "Received packet from an unknown inteface, address="
                      << remote_addr.ToString();
    return;
  }
  SimInterface* src_iface = it1->second;
  RTC_DCHECK(src_iface != nullptr);
  rtc::scoped_refptr<SimPacket> packet = new rtc::RefCountedObject<SimPacket>(
      reinterpret_cast<uint8_t*>(buffer_.data()), rv);
#if defined(USE_RAW_IP_PACKET)
  if (!packet->IsValid()) {
    RTC_LOG(LS_ERROR) << "Discarding an ill-formated packet from interface "
                      << iface->name();
    return;
  }
  // There can be spurious read from the system, which gets parsed as an IP
  // packet above.
  // TODO(qingsi): Fail the parsing of spurious read.
  if (packet->dst_ip().ToString() != iface->dual()->ip_str()) {
    RTC_LOG(INFO) << "before discarding dst ip = "
                  << packet->dst_ip().ToString();
    RTC_LOG(LS_ERROR) << "Discarding packet from spurious read from
        interface "
                      << iface->name();
    return;
  }
  packet->set_dst_ip(rtc::SocketAddress(iface->ip_str(), 0).ipaddr());
#endif
  iface->OnPacketReceived(std::move(packet), remote_addr, dst_port);
}

bool SimCore::CreateAndBindSocketOnDualInterface(SimInterface* iface,
                                                 int port_to_bind) {
  RTC_DCHECK(iface->role() == SimInterface::Role::kDual);
  int sockfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    RTC_LOG(LS_ERROR) << "Failed to create a dual socket for interface "
                      << iface->name();
    return false;
  }
  RTC_DCHECK(dual_socket_by_fd_.find(sockfd) == dual_socket_by_fd_.end());
  rtc::SocketAddress addr_to_bind(iface->ip(), port_to_bind);
  RTC_DCHECK(dual_socket_by_addr_.find(addr_to_bind.ToString()) ==
             dual_socket_by_addr_.end());
  SimPlatformSocket platform_socket(sockfd, addr_to_bind);
  sockaddr_storage saddr;
  size_t len = platform_socket.addr.ToSockAddrStorage(&saddr);
  int rv = ::bind(sockfd, reinterpret_cast<sockaddr*>(&saddr),
                  static_cast<int>(len));
  if (rv < 0) {
    RTC_LOG(LS_ERROR) << "Failed to bind a dual socket on interface "
                      << iface->name() << " to port " << port_to_bind
                      << ", err=" << errno;
  }
  dual_socket_by_fd_[sockfd] = platform_socket;
  dual_socket_by_addr_[addr_to_bind.ToString()] = platform_socket;
  iface->AddDualSocket(platform_socket);
  return true;
}

void SimCore::ReplayPacket(rtc::scoped_refptr<SimPacket> packet,
                           SimInterface* src_iface,
                           int src_port,
                           SimInterface* dst_iface,
                           int dst_port) {
  RTC_DCHECK(src_iface->role() == SimInterface::Role::kPrime);
  RTC_DCHECK(dst_iface->role() == SimInterface::Role::kPrime);

  const rtc::SocketAddress src_addr(src_iface->dual()->ip(), src_port);
  const auto& it = dual_socket_by_addr_.find(src_addr.ToString());
  RTC_DCHECK(it != dual_socket_by_addr_.end());
  int sockfd = it->second.sockfd;
  rtc::SocketAddress dst_addr(dst_iface->ip(), dst_port);
  sockaddr_storage saddr;
  size_t len = dst_addr.ToSockAddrStorage(&saddr);
  int rv = ::sendto(sockfd, packet->buffer().data(), packet->buffer().size(), 0,
                    reinterpret_cast<sockaddr*>(&saddr), static_cast<int>(len));
  if (rv < 0) {
    RTC_LOG(LS_ERROR) << "Failed to deliver packet, err=" << errno;
  }
}

}  // namespace webrtc
