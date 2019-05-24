/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_COMPOSITE_PACKET_TRANSPORT_H_
#define P2P_BASE_COMPOSITE_PACKET_TRANSPORT_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/socket.h"

namespace rtc {

// Composite packet transport capable of receiving from multiple sub-transports.
//
// Note that a composite is only capable of sending on a single component
// transport.  That transport must be chosen by out-of-band negotiation and set
// explicitly by calling |SetSendTransport|.  Until it is set, the composite is
// read-only.
class CompositePacketTransport : public PacketTransportInternal {
 public:
  explicit CompositePacketTransport(
      std::vector<PacketTransportInternal*> transports);

  // Sets which transport will be used to send.  |send_transport| must be one of
  // the composite's component transports.
  bool SetSendTransport(PacketTransportInternal* send_transport);

  // All transports within a composite must share the same transport name.
  const std::string& transport_name() const override;

  // A composite becomes writable once its send transport becomes writable.
  // Returns whether the send transport is writable, or false if the send
  // transport is unset.
  bool writable() const override;

  // A composite is receiving if any of the constituent transports are
  // receiving.
  bool receiving() const override;

  // Sends a packet.  May not be called until the send transport is set.
  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags = 0) override;

  // Sets options on all constituent transports.
  int SetOption(rtc::Socket::Option opt, int value) override;

  // Gets an option from the first transport that has a value for that option.
  //
  // All transports should be kept in sync by setting options through SetOption
  // on the composite, which sets it on all of them.  However, if some of the
  // constituent transports drop an option, this method will reflect the value
  // from the first transport that remembered it.
  bool GetOption(rtc::Socket::Option opt, int* value) override;

  // Gets the first error found among any of the constituent transports.
  int GetError() override;

  // Gets the network route of the first consituent transport.
  //
  // CompositePacketTransport is not intended for use with transports that have
  // different network routes, as there is no sensible way to reflect that
  // through the PacketTransportInternal interface.  All the sub-transports
  // should use the same ICE, and share the same network route.  However, in the
  // event that they do not, all network routes will be signaled through
  // SignalNetworkRouteChanged, but only the first transport's route will be
  // reflected here.
  absl::optional<NetworkRoute> network_route() const override;

 private:
  // Receive-side signal handlers.
  void OnReceivingState(PacketTransportInternal* transport);
  void OnReadPacket(PacketTransportInternal* transport,
                    const char* packet,
                    size_t size,
                    const int64_t& packet_time,
                    int flags);
  void OnNetworkRouteChanged(absl::optional<NetworkRoute> route);

  // Send-side signal handlers.
  void OnWritableState(PacketTransportInternal* transport);
  void OnReadyToSend(PacketTransportInternal* transport);
  void OnSentPacket(PacketTransportInternal* transport,
                    const rtc::SentPacket& packet);

  std::vector<PacketTransportInternal*> transports_;
  PacketTransportInternal* send_transport_ = nullptr;
  int error_ = 0;
};

}  // namespace rtc

#endif  // P2P_BASE_COMPOSITE_PACKET_TRANSPORT_H_
