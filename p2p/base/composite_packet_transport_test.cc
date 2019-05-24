/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/composite_packet_transport.h"

#include <memory>

#include "absl/memory/memory.h"
#include "p2p/base/fake_packet_transport.h"
#include "test/gtest.h"

namespace rtc {
namespace {

constexpr char kTransportName[] = "test_transport";

class CompositePacketTransportTest : public ::testing::Test,
                                     public sigslot::has_slots<> {
 public:
  CompositePacketTransportTest()
      : transport_1_(absl::make_unique<FakePacketTransport>(kTransportName)),
        transport_2_(absl::make_unique<FakePacketTransport>(kTransportName)),
        composite_(absl::make_unique<CompositePacketTransport>(
            std::vector<PacketTransportInternal*>{transport_1_.get(),
                                                  transport_2_.get()})),
        remote_(absl::make_unique<FakePacketTransport>("remote")) {
    composite_->SignalWritableState.connect(
        this, &CompositePacketTransportTest::OnWritableState);
    composite_->SignalReadyToSend.connect(
        this, &CompositePacketTransportTest::OnReadyToSend);
    composite_->SignalReceivingState.connect(
        this, &CompositePacketTransportTest::OnReceivingState);
    composite_->SignalReadPacket.connect(
        this, &CompositePacketTransportTest::OnReadPacket);
    composite_->SignalSentPacket.connect(
        this, &CompositePacketTransportTest::OnSentPacket);
    composite_->SignalNetworkRouteChanged.connect(
        this, &CompositePacketTransportTest::OnNetworkRouteChanged);
  }

  void OnWritableState(PacketTransportInternal* transport) {
    ++writable_state_count_;
  }

  void OnReadyToSend(PacketTransportInternal* transport) {
    ++ready_to_send_count_;
  }

  void OnReceivingState(PacketTransportInternal* transport) {
    ++receiving_state_count_;
  }

  void OnReadPacket(PacketTransportInternal* transport,
                    const char* data,
                    size_t size,
                    const int64_t& packet_time,
                    int flags) {
    ++read_packet_count_;
    last_packet_ = std::string(data, size);
    last_packet_time_ = packet_time;
    last_packet_flags_ = flags;
  }

  void OnSentPacket(PacketTransportInternal* transport,
                    const rtc::SentPacket& sent) {
    ++sent_packet_count_;
  }

  void OnNetworkRouteChanged(absl::optional<NetworkRoute> route) {
    ++network_route_count_;
    last_network_route_ = route;
  }

 protected:
  std::unique_ptr<FakePacketTransport> transport_1_;
  std::unique_ptr<FakePacketTransport> transport_2_;
  std::unique_ptr<CompositePacketTransport> composite_;

  std::unique_ptr<FakePacketTransport> remote_;

  int writable_state_count_ = 0;
  int ready_to_send_count_ = 0;
  int receiving_state_count_ = 0;
  int read_packet_count_ = 0;
  int sent_packet_count_ = 0;
  int network_route_count_ = 0;

  std::string last_packet_;
  int64_t last_packet_time_ = 0;
  int last_packet_flags_ = 0;
  absl::optional<NetworkRoute> last_network_route_;
};

TEST_F(CompositePacketTransportTest, TransportName) {
  EXPECT_EQ(kTransportName, composite_->transport_name());
}

TEST_F(CompositePacketTransportTest, NeverWritable) {
  transport_1_->SetWritable(true);
  transport_2_->SetWritable(true);

  EXPECT_FALSE(composite_->writable());
  EXPECT_EQ(0, writable_state_count_);
  EXPECT_EQ(0, ready_to_send_count_);
}

TEST_F(CompositePacketTransportTest, WritableWhenSendTransportWritable) {
  composite_->SetSendTransport(transport_2_.get());
  EXPECT_FALSE(composite_->writable());
  EXPECT_EQ(1, writable_state_count_);
  EXPECT_EQ(0, ready_to_send_count_);

  transport_1_->SetWritable(true);
  EXPECT_FALSE(composite_->writable());
  EXPECT_EQ(1, writable_state_count_);
  EXPECT_EQ(0, ready_to_send_count_);

  transport_2_->SetWritable(true);
  EXPECT_TRUE(composite_->writable());
  EXPECT_EQ(2, writable_state_count_);
  EXPECT_EQ(1, ready_to_send_count_);
}

TEST_F(CompositePacketTransportTest, SendTransportAlreadyReadyToSend) {
  transport_1_->SetWritable(true);
  composite_->SetSendTransport(transport_1_.get());
  EXPECT_TRUE(composite_->writable());
  EXPECT_EQ(1, writable_state_count_);
  EXPECT_EQ(1, ready_to_send_count_);
}

TEST_F(CompositePacketTransportTest, SetSendTransport) {
  EXPECT_TRUE(composite_->SetSendTransport(transport_1_.get()));
  EXPECT_TRUE(composite_->SetSendTransport(transport_2_.get()));

  // Already set, no op.
  EXPECT_TRUE(composite_->SetSendTransport(transport_2_.get()));

  // Not one of the component transports, no op.
  EXPECT_FALSE(composite_->SetSendTransport(remote_.get()));
}

TEST_F(CompositePacketTransportTest, ReceivingState) {
  transport_1_->SetReceiving(true);

  EXPECT_TRUE(composite_->receiving());
  EXPECT_EQ(1, receiving_state_count_);

  transport_1_->SetReceiving(false);

  EXPECT_FALSE(composite_->receiving());
  EXPECT_EQ(2, receiving_state_count_);

  transport_2_->SetReceiving(true);

  EXPECT_TRUE(composite_->receiving());
  EXPECT_EQ(3, receiving_state_count_);

  transport_2_->SetReceiving(false);

  EXPECT_FALSE(composite_->receiving());
  EXPECT_EQ(4, receiving_state_count_);
}

TEST_F(CompositePacketTransportTest, SetOption) {
  int value = 0;
  EXPECT_FALSE(composite_->GetOption(Socket::OPT_DSCP, &value));

  EXPECT_EQ(0, composite_->SetOption(Socket::OPT_DSCP, 2));

  EXPECT_TRUE(composite_->GetOption(Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);

  EXPECT_TRUE(transport_1_->GetOption(Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);

  EXPECT_TRUE(transport_2_->GetOption(Socket::OPT_DSCP, &value));
  EXPECT_EQ(value, 2);
}

TEST_F(CompositePacketTransportTest, NetworkRoute) {
  EXPECT_EQ(absl::nullopt, composite_->network_route());

  NetworkRoute route;
  route.local_network_id = 7;
  transport_1_->SetNetworkRoute(route);

  EXPECT_EQ(1, network_route_count_);
  EXPECT_EQ(last_network_route_->local_network_id, 7);
  EXPECT_EQ(composite_->network_route()->local_network_id, 7);

  // Note that transport_2_'s network route is still signaled, but not reflected
  // through network_route().  We expect CompositePacketTransport's
  // sub-transports to share the same ICE transport, so the chosen network route
  // should always match.
  route.local_network_id = 8;
  transport_2_->SetNetworkRoute(route);

  EXPECT_EQ(2, network_route_count_);
  EXPECT_EQ(last_network_route_->local_network_id, 8);
  EXPECT_NE(composite_->network_route()->local_network_id, 8);
}

TEST_F(CompositePacketTransportTest, GetError) {
  EXPECT_EQ(composite_->GetError(), 0);

  transport_1_->SetError(ENOTCONN);

  EXPECT_EQ(composite_->GetError(), ENOTCONN);
}

TEST_F(CompositePacketTransportTest, SendTransportNotSet) {
  std::string packet("foobar");
  EXPECT_EQ(composite_->SendPacket(packet.data(), packet.size(),
                                   rtc::PacketOptions(), 0),
            -1);
  EXPECT_EQ(composite_->GetError(), ENOTCONN);
  EXPECT_EQ(sent_packet_count_, 0);

  // The first call to GetError() clears the value.
  EXPECT_EQ(composite_->GetError(), 0);
}

TEST_F(CompositePacketTransportTest, SendOn1) {
  remote_->SetDestination(transport_1_.get(), false);
  composite_->SetSendTransport(transport_1_.get());

  std::string packet("foobar");
  EXPECT_EQ(composite_->SendPacket(packet.data(), packet.size(),
                                   rtc::PacketOptions(), 0),
            static_cast<int>(packet.size()));
  EXPECT_EQ(1, sent_packet_count_);
}

TEST_F(CompositePacketTransportTest, SendOn2) {
  remote_->SetDestination(transport_2_.get(), false);
  composite_->SetSendTransport(transport_2_.get());

  std::string packet("foobar");
  EXPECT_EQ(composite_->SendPacket(packet.data(), packet.size(),
                                   rtc::PacketOptions(), 0),
            static_cast<int>(packet.size()));
  EXPECT_EQ(1, sent_packet_count_);
}

TEST_F(CompositePacketTransportTest, ReceiveFrom1) {
  remote_->SetDestination(transport_1_.get(), false);

  std::string packet("foobar");
  remote_->SendPacket(packet.data(), packet.size(), PacketOptions(), 1);

  EXPECT_EQ(1, read_packet_count_);
  EXPECT_EQ(packet, last_packet_);
  EXPECT_GT(last_packet_time_, 0);
  EXPECT_EQ(0,
            last_packet_flags_);  // Flags are not propagated over the network.
}

TEST_F(CompositePacketTransportTest, ReceiveFrom2) {
  remote_->SetDestination(transport_2_.get(), false);

  std::string packet("foobar");
  remote_->SendPacket(packet.data(), packet.size(), PacketOptions(), 1);

  EXPECT_EQ(1, read_packet_count_);
  EXPECT_EQ(packet, last_packet_);
  EXPECT_GT(last_packet_time_, 0);
  EXPECT_EQ(0,
            last_packet_flags_);  // Flags are not propagated over the network.
}

}  // namespace
}  // namespace rtc
