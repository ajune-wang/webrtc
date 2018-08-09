/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basic_mdns_responder.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "rtc_base/bind.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"

namespace {

const int kMDnsPort = 5353;
const rtc::SocketAddress kMDnsMulticastAddressIpv4("224.0.0.251", kMDnsPort);
const rtc::SocketAddress kMDnsMulticastAddressIpv6("ff02::fb", kMDnsPort);
const int kMinimumIntervalBetweenResponsesMs = 1000;
// RFC 6762 Section 10.
const int kDefaultTtlForRecordWithHostnameMs = 120 * 1000;

rtc::SocketAddress GetResponseDestinationAddress(const rtc::SocketAddress& from,
                                                 bool prefer_unicast_response) {
  if (prefer_unicast_response) {
    return from;
  }
  if (from.family() == AF_INET) {
    return kMDnsMulticastAddressIpv4;
  } else if (from.family() == AF_INET6) {
    return kMDnsMulticastAddressIpv6;
  }
  RTC_NOTREACHED();
  return rtc::SocketAddress();
}

}  // namespace

namespace webrtc {

// TODO(qingsi): How to set SO_REUSEADDR for multicast address in WebRTC?
BasicMDnsResponder::BasicMDnsResponder(
    rtc::AsyncPacketSocket* listen_socket_ipv4,
    rtc::AsyncPacketSocket* listen_socket_ipv6,
    rtc::AsyncPacketSocket* send_socket)
    : listen_socket_ipv4_(listen_socket_ipv4),
      listen_socket_ipv6_(listen_socket_ipv6),
      send_socket_(send_socket) {
  if (!listen_socket_ipv4_ && !listen_socket_ipv6_) {
    RTC_LOG(LS_ERROR) << "No listening socket provided.";
    RTC_NOTREACHED();
  }
  if (!send_socket_) {
    RTC_LOG(LS_ERROR) << "No sending socket provided.";
    RTC_NOTREACHED();
  }
  if (listen_socket_ipv4_) {
    listen_socket_ipv4_->SignalReadPacket.connect(
        this, &BasicMDnsResponder::OnReadPacket);
  }
  if (listen_socket_ipv6_) {
    listen_socket_ipv6_->SignalReadPacket.connect(
        this, &BasicMDnsResponder::OnReadPacket);
  }
  send_socket_->SignalSentPacket.connect(this,
                                         &BasicMDnsResponder::OnResponseSent);
}

BasicMDnsResponder::~BasicMDnsResponder() = default;

std::string BasicMDnsResponder::CreateNameForAddress(
    const rtc::IPAddress& address) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (name_by_ip_.find(address) == name_by_ip_.end()) {
    name_by_ip_[address] = rtc::CreateRandomUuid() + ".local.";
  }
  return name_by_ip_[address];
}

void BasicMDnsResponder::OnQueryReceived(
    uint16_t query_id,
    const rtc::SocketAddress& from,
    const std::set<std::string>& names_to_resolve,
    bool prefer_unicast_response) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  std::map<std::string, rtc::IPAddress> resolution;
  for (auto it = name_by_ip_.begin(); it != name_by_ip_.end(); ++it) {
    if (names_to_resolve.find(it->second) != names_to_resolve.end()) {
      resolution[it->second] = it->first;
    }
  }
  if (resolution.empty()) {
    return;
  }
  // Response ID should be the same as the ID of the corresponding query.
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, rtc::Thread::Current(),
      rtc::Bind(&BasicMDnsResponder::OnResponseReadyToSend, this, query_id,
                GetResponseDestinationAddress(from, prefer_unicast_response),
                resolution),
      std::max(last_time_response_sent_ + kMinimumIntervalBetweenResponsesMs -
                   rtc::TimeMillis(),
               0l));
}

void BasicMDnsResponder::OnResponseReadyToSend(
    uint16_t response_id,
    const rtc::SocketAddress& to,
    const std::map<std::string, rtc::IPAddress>& resolution) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  MDnsMessage response;
  response.SetId(response_id);
  response.SetQueryOrResponse(false);
  response.SetAuthoritative(true);
  for (const auto& name_ip_pair : resolution) {
    MDnsResourceRecord answer;
    answer.SetName(name_ip_pair.first);
    if (name_ip_pair.second.family() == AF_INET) {
      answer.SetType(SectionDataType::kA);
    } else if (name_ip_pair.second.family() == AF_INET6) {
      answer.SetType(SectionDataType::kAAAA);
    } else {
      RTC_NOTREACHED();
    }
    answer.SetClass(SectionDataClass::kIN);
    answer.set_ttl_seconds(kDefaultTtlForRecordWithHostnameMs / 1000);
    answer.SetIPAddressInRecordData(name_ip_pair.second);
    response.AddAnswerRecord(answer);
  }

  rtc::ByteBufferWriter buf;
  response.Write(&buf);
  auto err =
      send_socket_->SendTo(buf.Data(), buf.Length(), to, rtc::PacketOptions());
  if (err < 0) {
    RTC_LOG(LS_ERROR) << "Failed to send mDNS resolution for names: ";
    for (const auto& name_ip_pair : resolution) {
      RTC_LOG(LS_ERROR) << name_ip_pair.first;
    }
    RTC_LOG(LS_ERROR) << "Socket error = " << send_socket_->GetError();
  }
}

void BasicMDnsResponder::OnReadPacket(rtc::AsyncPacketSocket* socket,
                                      const char* data,
                                      size_t size,
                                      const rtc::SocketAddress& remote_address,
                                      const rtc::PacketTime& packet_time) {
  MDnsMessage query;
  rtc::ByteBufferReader buf(data, size);
  if (!query.Read(&buf) || !query.IsQuery()) {
    return;
  }
  std::set<std::string> names_to_resolve;
  if (!query.GetNumQuestions()) {
    return;
  }
  int num_questions = query.GetNumQuestions();
  RTC_DCHECK(num_questions > 0);
  for (int i = 0; i < num_questions; ++i) {
    names_to_resolve.insert(query.GetQuestion(i)->GetName());
  }
  OnQueryReceived(query.GetId(), remote_address, names_to_resolve,
                  query.ShouldUnicastResponse());
}

void BasicMDnsResponder::OnResponseSent(rtc::AsyncPacketSocket* socket,
                                        const rtc::SentPacket& packet) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  last_time_response_sent_ = packet.send_time_ms;
  SignalResponseSent();
}

}  // namespace webrtc
