/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <memory>

#include "absl/memory/memory.h"
#include "api/test/simulated_network.h"
#include "call/simulated_network.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/network_emulation_manager.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kNetworkPacketWaitTimeoutMs = 100;

class SocketReader : public sigslot::has_slots<> {
 public:
  explicit SocketReader(rtc::AsyncSocket* socket) : socket_(socket) {
    socket_->SignalReadEvent.connect(this, &SocketReader::OnReadEvent);
    size_ = 128 * 1024;
    buf_ = new char[size_];
  }
  ~SocketReader() override { delete[] buf_; }

  void OnReadEvent(rtc::AsyncSocket* socket) {
    RTC_DCHECK(socket_ == socket);
    int64_t timestamp;
    len_ = socket_->Recv(buf_, size_, &timestamp);
    {
      rtc::CritScope crit(&lock_);
      received_count_++;
    }
  }

  int ReceivedCount() {
    rtc::CritScope crit(&lock_);
    return received_count_;
  }

 private:
  rtc::AsyncSocket* socket_;
  char* buf_;
  size_t size_;
  int len_;

  rtc::CriticalSection lock_;
  int received_count_ RTC_GUARDED_BY(lock_) = 0;
};

class MockReceiver : public EmulatedNetworkReceiverInterface {
 public:
  MockReceiver() = default;
  ~MockReceiver() = default;
  MOCK_METHOD1(OnPacketReceived, void(EmulatedIpPacket packet));
};

}  // namespace

using testing::_;

TEST(NetworkEmulationManagerTest, GeneratedIpv4AddressDoesNotCollide) {
  NetworkEmulationManagerImpl network_manager;
  std::set<rtc::IPAddress> ips;
  EmulatedEndpointConfig config;
  config.generated_ip_family = EmulatedEndpointConfig::IpAddressFamily::kIpv4;
  for (int i = 0; i < 1000; i++) {
    EmulatedEndpoint* endpoint = network_manager.CreateEndpoint(config);
    ASSERT_EQ(endpoint->GetPeerLocalAddress().family(), AF_INET);
    bool result = ips.insert(endpoint->GetPeerLocalAddress()).second;
    ASSERT_TRUE(result);
  }
}

TEST(NetworkEmulationManagerTest, GeneratedIpv6AddressDoesNotCollide) {
  NetworkEmulationManagerImpl network_manager;
  std::set<rtc::IPAddress> ips;
  EmulatedEndpointConfig config;
  config.generated_ip_family = EmulatedEndpointConfig::IpAddressFamily::kIpv6;
  for (int i = 0; i < 1000; i++) {
    EmulatedEndpoint* endpoint = network_manager.CreateEndpoint(config);
    ASSERT_EQ(endpoint->GetPeerLocalAddress().family(), AF_INET6);
    bool result = ips.insert(endpoint->GetPeerLocalAddress()).second;
    ASSERT_TRUE(result);
  }
}

TEST(NetworkEmulationManagerTest, Run) {
  NetworkEmulationManagerImpl network_manager;

  EmulatedNetworkNode* alice_node = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_manager.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  network_manager.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* nt1 =
      network_manager.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* nt2 =
      network_manager.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  for (uint64_t j = 0; j < 2; j++) {
    auto* s1 = nt1->network_thread()->socketserver()->CreateAsyncSocket(
        AF_INET, SOCK_DGRAM);
    auto* s2 = nt2->network_thread()->socketserver()->CreateAsyncSocket(
        AF_INET, SOCK_DGRAM);

    SocketReader r1(s1);
    SocketReader r2(s2);

    rtc::SocketAddress a1(alice_endpoint->GetPeerLocalAddress(), 0);
    rtc::SocketAddress a2(bob_endpoint->GetPeerLocalAddress(), 0);

    s1->Bind(a1);
    s2->Bind(a2);

    s1->Connect(s2->GetLocalAddress());
    s2->Connect(s1->GetLocalAddress());

    rtc::CopyOnWriteBuffer data("Hello");
    for (uint64_t i = 0; i < 1000; i++) {
      s1->Send(data.data(), data.size());
      s2->Send(data.data(), data.size());
    }

    rtc::Event wait;
    wait.Wait(1000);
    ASSERT_EQ(r1.ReceivedCount(), 1000);
    ASSERT_EQ(r2.ReceivedCount(), 1000);

    delete s1;
    delete s2;
  }
}

// Test connectivity of routing scheme with 3 endpoints e1, e2, e3 and
// 4 network nodes n1, n2, n3 and n4, which are connected with there routes:
//  * e1 -> n1 -> e2
//  * e2 -> n2 -> e1
//  * e1 -> n3 -> e3
//  * e3 -> n4 -> e1
//  _____     ____     ____
// | e1  |-->| n1 |-->| e2 |
// |     |    ˉˉˉˉ    |    |
// |     |    ____    |    |
// |     |<--| n2 |<--|    |
//  ˉ|ˉ|ˉ     ˉˉˉˉ     ˉˉˉˉ
//   | |      ____     ____
//   | └---->| n3 |-->| e3 |
//   |        ˉˉˉˉ    |    |
//   |        ____    |    |
//   └------>| n4 |<--|    |
//            ˉˉˉˉ     ˉˉˉˉ
TEST(NetworkEmulationManagerTest, ComplexRouting) {
  NetworkEmulationManagerImpl emulation;

  // Create 4 network nodes.
  EmulatedNetworkNode* n1 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* n2 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* n3 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* n4 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));

  // Create 3 endpoints.
  EmulatedEndpoint* e1 = emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* e2 = emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* e3 = emulation.CreateEndpoint(EmulatedEndpointConfig());

  // Establish routing.
  emulation.CreateRoute(e1, {n1}, e2);
  emulation.CreateRoute(e2, {n2}, e1);

  emulation.CreateRoute(e1, {n3}, e3);
  emulation.CreateRoute(e3, {n4}, e1);

  // Receivers: r_<source endpoint>_<destination endpoint>
  MockReceiver r_e1_e2;
  MockReceiver r_e2_e1;
  MockReceiver r_e1_e3;
  MockReceiver r_e3_e1;

  EXPECT_CALL(r_e1_e2, OnPacketReceived(_)).Times(1);
  EXPECT_CALL(r_e2_e1, OnPacketReceived(_)).Times(1);
  EXPECT_CALL(r_e1_e3, OnPacketReceived(_)).Times(1);
  EXPECT_CALL(r_e3_e1, OnPacketReceived(_)).Times(1);

  uint16_t common_send_port = 80;
  uint16_t r_e1_e2_port = e2->BindReceiver(0, &r_e1_e2).value();
  uint16_t r_e2_e1_port = e1->BindReceiver(0, &r_e2_e1).value();
  uint16_t r_e1_e3_port = e3->BindReceiver(0, &r_e1_e3).value();
  uint16_t r_e3_e1_port = e1->BindReceiver(0, &r_e3_e1).value();

  // Send packet from e1 to e2.
  e1->SendPacket(
      rtc::SocketAddress(e1->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e2->GetPeerLocalAddress(), r_e1_e2_port),
      rtc::CopyOnWriteBuffer(10));

  // Send packet from e2 to e1.
  e2->SendPacket(
      rtc::SocketAddress(e2->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e1->GetPeerLocalAddress(), r_e2_e1_port),
      rtc::CopyOnWriteBuffer(10));

  // Send packet from e1 to e3.
  e1->SendPacket(
      rtc::SocketAddress(e1->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e3->GetPeerLocalAddress(), r_e1_e3_port),
      rtc::CopyOnWriteBuffer(10));

  // Send packet from e3 to e1.
  e3->SendPacket(
      rtc::SocketAddress(e3->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e1->GetPeerLocalAddress(), r_e3_e1_port),
      rtc::CopyOnWriteBuffer(10));

  // Sleep at the end to wait for async packets delivery.
  SleepMs(kNetworkPacketWaitTimeoutMs);
}

// Test connectivity of routing scheme with 3 endpoints e1, e2, e3 and
// 3 network nodes n1, n2 and n3, which are connected with there routes:
//  * e1 -> n1 -> e2
//  * e1 -> n1 -> e3
//  * e2 -> n2 -> e1
//  * e3 -> n3 -> e1
//  _____     ____     ____
// | e1  |<--| n2 |<--| e2 |
// |     |    ˉˉˉˉ    |    |
// |     |    ____    |    |
// |     |-->| n1 |-->|    |
//  ˉ|ˉ|ˉ    |    |    ˉˉˉˉ
//   | |     |    |    ____
//   | └---->|    |-->| e3 |
//   |        ˉˉˉˉ    |    |
//   |        ____    |    |
//   └------>| n3 |<--|    |
//            ˉˉˉˉ     ˉˉˉˉ
TEST(NetworkEmulationManagerTest, ComplexRoutingReuse) {
  NetworkEmulationManagerImpl emulation;

  // Create 3 network nodes.
  EmulatedNetworkNode* n1 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* n2 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* n3 = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));

  // Create 3 endpoints.
  EmulatedEndpoint* e1 = emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* e2 = emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* e3 = emulation.CreateEndpoint(EmulatedEndpointConfig());

  // Establish routing.
  emulation.CreateRoute(e1, {n1}, e2);
  emulation.CreateRoute(e2, {n2}, e1);

  emulation.CreateRoute(e1, {n1}, e3);
  emulation.CreateRoute(e3, {n3}, e1);

  // Receivers: r_<source endpoint>_<destination endpoint>
  MockReceiver r_e1_e2;
  MockReceiver r_e2_e1;
  MockReceiver r_e1_e3;
  MockReceiver r_e3_e1;

  EXPECT_CALL(r_e1_e2, OnPacketReceived(_)).Times(1);
  EXPECT_CALL(r_e2_e1, OnPacketReceived(_)).Times(1);
  EXPECT_CALL(r_e1_e3, OnPacketReceived(_)).Times(1);
  EXPECT_CALL(r_e3_e1, OnPacketReceived(_)).Times(1);

  uint16_t common_send_port = 80;
  uint16_t r_e1_e2_port = e2->BindReceiver(0, &r_e1_e2).value();
  uint16_t r_e2_e1_port = e1->BindReceiver(0, &r_e2_e1).value();
  uint16_t r_e1_e3_port = e3->BindReceiver(0, &r_e1_e3).value();
  uint16_t r_e3_e1_port = e1->BindReceiver(0, &r_e3_e1).value();

  // Send packet from e1 to e2.
  e1->SendPacket(
      rtc::SocketAddress(e1->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e2->GetPeerLocalAddress(), r_e1_e2_port),
      rtc::CopyOnWriteBuffer(10));

  // Send packet from e2 to e1.
  e2->SendPacket(
      rtc::SocketAddress(e2->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e1->GetPeerLocalAddress(), r_e2_e1_port),
      rtc::CopyOnWriteBuffer(10));

  // Send packet from e1 to e3.
  e1->SendPacket(
      rtc::SocketAddress(e1->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e3->GetPeerLocalAddress(), r_e1_e3_port),
      rtc::CopyOnWriteBuffer(10));

  // Send packet from e3 to e1.
  e3->SendPacket(
      rtc::SocketAddress(e3->GetPeerLocalAddress(), common_send_port),
      rtc::SocketAddress(e1->GetPeerLocalAddress(), r_e3_e1_port),
      rtc::CopyOnWriteBuffer(10));

  // Sleep at the end to wait for async packets delivery.
  SleepMs(kNetworkPacketWaitTimeoutMs);
}

}  // namespace test
}  // namespace webrtc
