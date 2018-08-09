/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "p2p/base/basic_mdns_responder.h"

#include "p2p/base/basicpacketsocketfactory.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

namespace {

const int kMDnsPort = 5353;
const rtc::SocketAddress kMDnsMulticastAddressIpv4("224.0.0.251", kMDnsPort);
const rtc::SocketAddress kPublicAddressIPv4[2] = {
    rtc::SocketAddress("11.11.11.11", 0), rtc::SocketAddress("22.22.22.22", 0)};
const rtc::SocketAddress kPrivateAddressIPv4 =
    rtc::SocketAddress("192.168.0.1", 0);
const rtc::SocketAddress kPrivateAddressIPv6 =
    rtc::SocketAddress("fd12:3456:789a:1::1", 0);
const int kDefaultTimeout = 2000;
const int kLongTimeout = 20000;

}  // namespace

namespace webrtc {

// Currently there is a caveat in rtc::PacketSocketFactory that we
// do not have an explicit API to create sockets that reuse a socket address;
// also, there is no API to join a multicast group. Our implementation of
// rtc::PacketSocketFactory, including rtc::BasicPacketSocketFactory, does not
// provide the support of address reuse and group subscription either. As a
// result, we cannot set up a scenario with two or more sockets listening for
// mDNS packets sent by multicast in the same address family. Hence, we let the
// querier (which is really the test fixture) and the responder share the same
// rtc::AsyncPacketSocket object, bound to a multicast address (to emulate the
// subscription to a multicast group) in the tests for an address family.
//
// TODO(qingsi): We need to change the rtc::PacketSocketFactory and the
// rtc::AsyncPacketSocket interfaces to support multicast.
class BasicMDnsResponderTest : public testing::Test,
                               public sigslot::has_slots<> {
 public:
  BasicMDnsResponderTest()
      : vss_(new rtc::VirtualSocketServer()),
        pss_(new rtc::BasicPacketSocketFactory(vss_.get())),
        thread_(vss_.get()),
        multicast_socket_(pss_->CreateUdpSocket(kMDnsMulticastAddressIpv4,
                                                kMDnsPort,
                                                kMDnsPort)),
        responder_send_socket_(
            pss_->CreateUdpSocket(kPublicAddressIPv4[0], 0, 0)),
        querier_send_socket_(
            pss_->CreateUdpSocket(kPublicAddressIPv4[1], 0, 0)),
        mdns_responder_(new BasicMDnsResponder(multicast_socket_.get(),
                                               nullptr,
                                               responder_send_socket_.get())) {
    multicast_socket_->SignalReadPacket.connect(
        this, &BasicMDnsResponderTest::OnPacketReceived);
    querier_send_socket_->SignalReadPacket.connect(
        this, &BasicMDnsResponderTest::OnPacketReceived);
  }

  std::string RegisterNameAtResponder(const rtc::IPAddress& address) {
    return mdns_responder_->CreateNameForAddress(address);
  }

  MDnsMessage CreateQuery(const std::vector<std::string>& names,
                          int query_id,
                          bool unicast_response) {
    MDnsMessage msg;
    msg.SetId(query_id);
    msg.SetQueryOrResponse(true);

    for (const auto& name : names) {
      MDnsQuestion question;
      question.SetName(name);
      question.SetType(SectionDataType::kA);
      question.SetClass(SectionDataClass::kIN);
      question.SetUnicastResponse(unicast_response);
      msg.AddQuestion(question);
    }
    return msg;
  }

  void ReceiveQueryAndSendResponse(const MDnsMessage& query,
                                   rtc::AsyncPacketSocket* from) {
    rtc::ByteBufferWriter buf;
    EXPECT_TRUE(query.Write(&buf));
    RTC_DCHECK(from->GetLocalAddress().family() == AF_INET);
    multicast_socket_->SignalReadPacket(multicast_socket_.get(), buf.Data(),
                                        buf.Length(), from->GetLocalAddress(),
                                        rtc::PacketTime());
  }

  void OnPacketReceived(rtc::AsyncPacketSocket* socket,
                        const char* data,
                        size_t size,
                        const rtc::SocketAddress& from,
                        const rtc::PacketTime& packet_time) {
    rtc::ByteBufferReader buf(data, size);
    MDnsMessage msg;
    EXPECT_TRUE(msg.Read(&buf));
    if (msg.IsQuery()) {
      // This is a query and let it be handled by the responder.
      return;
    }
    last_response_received_ = msg;
    OnResponseReceived(socket, msg, from);
  }

  void OnResponseReceived(rtc::AsyncPacketSocket* socket,
                          const MDnsMessage& response,
                          const rtc::SocketAddress& from) {
    response_received_ = true;
    last_response_received_ = response;
    bool should_unicast_response =
        last_query_sent_.GetQuestion(0)->ShouldUnicastResponse();
    const rtc::SocketAddress& expected_response_dest_address =
        should_unicast_response ? querier_send_socket_->GetLocalAddress()
                                : kMDnsMulticastAddressIpv4;
    EXPECT_EQ(expected_response_dest_address, socket->GetLocalAddress());
  }

  void CheckLastResponse(
      int expected_response_id,
      std::map<std::string, rtc::IPAddress> expected_resolution) {
    EXPECT_EQ(expected_response_id, last_response_received_.GetId());
    size_t num_answers = last_response_received_.GetNumAnswerRecords();
    EXPECT_EQ(expected_resolution.size(), num_answers);
    for (size_t i = 0; i < num_answers; ++i) {
      MDnsResourceRecord* answer = last_response_received_.GetAnswerRecord(i);
      ASSERT_TRUE(answer != nullptr);
      EXPECT_NE(expected_resolution.end(),
                expected_resolution.find(answer->GetName()));
      const rtc::IPAddress expected_resolved_address =
          expected_resolution[answer->GetName()];
      const SectionDataType expected_answer_type =
          expected_resolved_address.family() == AF_INET
              ? SectionDataType::kA
              : SectionDataType::kAAAA;
      EXPECT_EQ(expected_answer_type, answer->GetType());
      EXPECT_EQ(SectionDataClass::kIN, answer->GetClass());
      rtc::IPAddress addr;
      EXPECT_TRUE(answer->GetIPAddressFromRecordData(&addr));
      EXPECT_EQ(expected_resolved_address, addr);
    }
  }

 protected:
  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> pss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<rtc::AsyncPacketSocket> multicast_socket_;
  std::unique_ptr<rtc::AsyncPacketSocket> responder_send_socket_;
  std::unique_ptr<rtc::AsyncPacketSocket> querier_send_socket_;
  std::unique_ptr<BasicMDnsResponder> mdns_responder_;
  MDnsMessage last_query_sent_;
  MDnsMessage last_response_received_;
  bool response_received_ = false;
};

TEST_F(BasicMDnsResponderTest, NoReplyToQueryWithUnknowNames) {
  rtc::ScopedFakeClock clock;
  last_query_sent_ = CreateQuery({"foo", "bar"}, 123, false);
  ReceiveQueryAndSendResponse(last_query_sent_, querier_send_socket_.get());
  SIMULATED_WAIT(false, kLongTimeout, clock);
  EXPECT_FALSE(response_received_);
}

TEST_F(BasicMDnsResponderTest, ReplyUnicastResponseToQueryWithSingleQuestion) {
  rtc::ScopedFakeClock clock;
  std::string name = RegisterNameAtResponder(kPrivateAddressIPv4.ipaddr());
  last_query_sent_ = CreateQuery({name}, 123, true);
  ReceiveQueryAndSendResponse(last_query_sent_, querier_send_socket_.get());
  EXPECT_TRUE_SIMULATED_WAIT(response_received_, kDefaultTimeout, clock);
  std::map<std::string, rtc::IPAddress> expected_resolution = {
      {name, kPrivateAddressIPv4.ipaddr()}};
  CheckLastResponse(123, expected_resolution);
}

TEST_F(BasicMDnsResponderTest, ReplyMulticastResponseToQueryWithTwoQuestions) {
  rtc::ScopedFakeClock clock;
  std::string name_ipv4 = RegisterNameAtResponder(kPrivateAddressIPv4.ipaddr());
  std::string name_ipv6 = RegisterNameAtResponder(kPrivateAddressIPv6.ipaddr());
  last_query_sent_ = CreateQuery({name_ipv4, name_ipv6}, 123, false);
  ReceiveQueryAndSendResponse(last_query_sent_, querier_send_socket_.get());
  EXPECT_TRUE_SIMULATED_WAIT(response_received_, kDefaultTimeout, clock);
  std::map<std::string, rtc::IPAddress> expected_resolution = {
      {name_ipv4, kPrivateAddressIPv4.ipaddr()},
      {name_ipv6, kPrivateAddressIPv6.ipaddr()}};
  CheckLastResponse(123, expected_resolution);
}

}  // namespace webrtc
