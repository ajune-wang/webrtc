/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_FAKEPORTALLOCATOR_H_
#define P2P_BASE_FAKEPORTALLOCATOR_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p2p/base/basicpacketsocketfactory.h"
#include "p2p/base/portallocator.h"
#include "p2p/base/udpport.h"
#include "rtc_base/nethelpers.h"

namespace rtc {
class SocketFactory;
class Thread;
}

namespace cricket {

class TestUDPPort : public UDPPort {
 public:
  static TestUDPPort* Create(
      rtc::Thread* thread,
      rtc::PacketSocketFactory* factory,
      rtc::Network* network,
      uint16_t min_port,
      uint16_t max_port,
      const std::string& username,
      const std::string& password,
      const std::string& origin,
      bool emit_localhost_for_anyaddress,
      rtc::Optional<int> stun_keepalive_interval = rtc::nullopt) {
    TestUDPPort* port =
        new TestUDPPort(thread, factory, network, min_port, max_port, username,
                        password, origin, emit_localhost_for_anyaddress);
    port->set_stun_keepalive_delay(stun_keepalive_interval);
    if (!port->Init()) {
      delete port;
      port = nullptr;
    }
    return port;
  }
  void SendBindingResponse(StunMessage* request,
                           const rtc::SocketAddress& addr) override {
    UDPPort::SendBindingResponse(request, addr);
    sent_binding_response_ = true;
  }
  bool sent_binding_response() { return sent_binding_response_; }
  void set_sent_binding_response(bool response) {
    sent_binding_response_ = response;
  }

 protected:
  TestUDPPort(rtc::Thread* thread,
              rtc::PacketSocketFactory* factory,
              rtc::Network* network,
              uint16_t min_port,
              uint16_t max_port,
              const std::string& username,
              const std::string& password,
              const std::string& origin,
              bool emit_localhost_for_anyaddress)
      : UDPPort(thread,
                factory,
                network,
                min_port,
                max_port,
                username,
                password,
                origin,
                emit_localhost_for_anyaddress) {}

  bool sent_binding_response_ = false;
};

enum class PortTypeToAllocateInFakeSession {
  TEST_UDP,
  UDP,
  STUN,
};

typedef std::set<PortTypeToAllocateInFakeSession> PortTypesForFakeSession;

// A FakePortAllocatorSession can be used with either a real or fake socket
// factory. It gathers a single loopback port, using IPv6 if available and
// not disabled.
class FakePortAllocatorSession : public PortAllocatorSession {
 public:
  FakePortAllocatorSession(PortAllocator* allocator,
                           rtc::Thread* network_thread,
                           rtc::PacketSocketFactory* factory,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_ufrag,
                           const std::string& ice_pwd)
      : PortAllocatorSession(content_name,
                             component,
                             ice_ufrag,
                             ice_pwd,
                             allocator->flags()),
        allocator_(allocator),
        network_thread_(network_thread),
        factory_(factory),
        ipv4_network_("network",
                      "unittest",
                      rtc::IPAddress(INADDR_LOOPBACK),
                      32),
        ipv6_network_("network",
                      "unittest",
                      rtc::IPAddress(in6addr_loopback),
                      64),
        ports_(),
        port_config_count_(0),
        stun_servers_(allocator->stun_servers()),
        turn_servers_(allocator->turn_servers()) {
    ipv4_network_.AddIP(rtc::IPAddress(INADDR_LOOPBACK));
    ipv6_network_.AddIP(rtc::IPAddress(in6addr_loopback));
  }

  void SetCandidateFilter(uint32_t filter) override {
    candidate_filter_ = filter;
  }

  void set_allocation_sequence(PortTypesForFakeSession allocation_sequence) {
    allocation_sequence_ = allocation_sequence;
  }

  void StartGettingPorts() override {
    if (ports_.empty()) {
      rtc::Network& network =
          (rtc::HasIPv6Enabled() && (flags() & PORTALLOCATOR_ENABLE_IPV6))
              ? ipv6_network_
              : ipv4_network_;
      for (auto port_type : allocation_sequence_) {
        std::unique_ptr<cricket::Port> port;
        switch (port_type) {
          case PortTypeToAllocateInFakeSession::TEST_UDP: {
            port.reset(TestUDPPort::Create(
                network_thread_, factory_, &network, 0, 0, username(),
                password(), std::string(), false,
                allocator_->stun_candidate_keepalive_interval()));
            break;
          }
          case PortTypeToAllocateInFakeSession::UDP: {
            port.reset(UDPPort::Create(
                network_thread_, factory_, &network, 0, 0, username(),
                password(), std::string(), false,
                allocator_->stun_candidate_keepalive_interval()));
            break;
          }
          case PortTypeToAllocateInFakeSession::STUN: {
            ServerAddresses stun_servers;
            port.reset(StunPort::Create(
                network_thread_, factory_, &network, 0, 0, username(),
                password(), stun_servers, std::string(),
                allocator_->stun_candidate_keepalive_interval()));
            break;
          }
          default:
            RTC_NOTREACHED();
        }
        ports_.push_back(std::move(port));
        ports_.back()->SignalDestroyed.connect(
            this, &FakePortAllocatorSession::OnPortDestroyed);
        AddPort(ports_.back().get());
      }
    }
    ++port_config_count_;
    running_ = true;
  }

  void StopGettingPorts() override { running_ = false; }
  bool IsGettingPorts() override { return running_; }
  void ClearGettingPorts() override {}

  std::vector<PortInterface*> ReadyPorts() const override {
    return ready_ports_;
  }
  std::vector<Candidate> ReadyCandidates() const override {
    return candidates_;
  }
  void PruneAllPorts() override {
    for (const auto& port : ports_) {
      port->Prune();
    }
  }
  bool CandidatesAllocationDone() const override { return allocation_done_; }

  // TODO(qingsi): This is a duplicate of the same method in
  // BasicPortAllocatorSession.
  void SetStunKeepaliveIntervalForReadyPorts(
      const rtc::Optional<int>& stun_keepalive_interval) override {
    auto ports = ReadyPorts();
    for (PortInterface* port : ports) {
      if (port->Type() == STUN_PORT_TYPE ||
          (port->Type() == LOCAL_PORT_TYPE &&
           port->GetProtocol() == PROTO_UDP)) {
        static_cast<UDPPort*>(port)->set_stun_keepalive_delay(
            stun_keepalive_interval);
      }
    }
  }

  int port_config_count() { return port_config_count_; }

  const ServerAddresses& stun_servers() const { return stun_servers_; }

  const std::vector<RelayServerConfig>& turn_servers() const {
    return turn_servers_;
  }

  uint32_t candidate_filter() const { return candidate_filter_; }

  int transport_info_update_count() const {
    return transport_info_update_count_;
  }

 protected:
  void UpdateIceParametersInternal() override {
    // Since this class is a fake and this method only is overridden for tests,
    // we don't need to actually update the transport info.
    ++transport_info_update_count_;
  }

 private:
  void AddPort(cricket::Port* port) {
    port->set_component(component());
    port->set_generation(generation());
    port->SignalPortComplete.connect(this,
                                     &FakePortAllocatorSession::OnPortComplete);
    port->PrepareAddress();
    ready_ports_.push_back(port);
    SignalPortReady(this, port);
    port->KeepAliveUntilPruned();
  }
  void OnPortComplete(cricket::Port* port) {
    const std::vector<Candidate>& candidates = port->Candidates();
    candidates_.insert(candidates_.end(), candidates.begin(), candidates.end());
    SignalCandidatesReady(this, candidates);

    allocation_done_ = true;
    SignalCandidatesAllocationDone(this);
  }
  void OnPortDestroyed(cricket::PortInterface* port) {
    // Don't want to double-delete port if it deletes itself.
    for (auto& p : ports_) {
      p.release();
    }
  }

  PortAllocator* allocator_;
  rtc::Thread* network_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::Network ipv4_network_;
  rtc::Network ipv6_network_;
  std::vector<std::unique_ptr<cricket::Port>> ports_;
  int port_config_count_;
  std::vector<Candidate> candidates_;
  std::vector<PortInterface*> ready_ports_;
  bool allocation_done_ = false;
  ServerAddresses stun_servers_;
  std::vector<RelayServerConfig> turn_servers_;
  uint32_t candidate_filter_ = CF_ALL;
  int transport_info_update_count_ = 0;
  bool running_ = false;
  PortTypesForFakeSession allocation_sequence_;
};

class FakePortAllocator : public cricket::PortAllocator {
 public:
  FakePortAllocator(rtc::Thread* network_thread,
                    rtc::PacketSocketFactory* factory)
      : network_thread_(network_thread), factory_(factory) {
    if (factory_ == NULL) {
      owned_factory_.reset(new rtc::BasicPacketSocketFactory(network_thread_));
      factory_ = owned_factory_.get();
    }
    allocation_sequence.insert(PortTypeToAllocateInFakeSession::TEST_UDP);
  }

  void Initialize() override {
    // Port allocator should be initialized on the network thread.
    RTC_CHECK(network_thread_->IsCurrent());
    initialized_ = true;
  }

  void SetNetworkIgnoreMask(int network_ignore_mask) override {}

  void AddPortTypeToAllocationSequence(PortTypeToAllocateInFakeSession type) {
    allocation_sequence.insert(type);
  }

  cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override {
    auto session = new FakePortAllocatorSession(this, network_thread_, factory_,
                                                content_name, component,
                                                ice_ufrag, ice_pwd);
    session->set_allocation_sequence(allocation_sequence);
    return session;
  }

  bool initialized() const { return initialized_; }

 private:
  rtc::Thread* network_thread_;
  rtc::PacketSocketFactory* factory_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> owned_factory_;
  bool initialized_ = false;
  PortTypesForFakeSession allocation_sequence;
};

}  // namespace cricket

#endif  // P2P_BASE_FAKEPORTALLOCATOR_H_
