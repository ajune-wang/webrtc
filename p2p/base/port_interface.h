/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PORT_INTERFACE_H_
#define P2P_BASE_PORT_INTERFACE_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/packet_socket_factory.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

namespace rtc {
class Network;
struct PacketOptions;
}  // namespace rtc

namespace cricket {
class ConnectionInterface;
class IceMessage;
class StunMessage;
class StunStats;

enum ProtocolType {
  PROTO_UDP,
  PROTO_TCP,
  PROTO_SSLTCP,  // Pseudo-TLS.
  PROTO_TLS,
  PROTO_LAST = PROTO_TLS
};

RTC_EXPORT extern const char LOCAL_PORT_TYPE[];
RTC_EXPORT extern const char STUN_PORT_TYPE[];
RTC_EXPORT extern const char PRFLX_PORT_TYPE[];
RTC_EXPORT extern const char RELAY_PORT_TYPE[];

// RFC 6544, TCP candidate encoding rules.
extern const int DISCARD_PORT;
extern const char TCPTYPE_ACTIVE_STR[];
extern const char TCPTYPE_PASSIVE_STR[];
extern const char TCPTYPE_SIMOPEN_STR[];

enum IcePriorityValue {
  ICE_TYPE_PREFERENCE_RELAY_TLS = 0,
  ICE_TYPE_PREFERENCE_RELAY_TCP = 1,
  ICE_TYPE_PREFERENCE_RELAY_UDP = 2,
  ICE_TYPE_PREFERENCE_PRFLX_TCP = 80,
  ICE_TYPE_PREFERENCE_HOST_TCP = 90,
  ICE_TYPE_PREFERENCE_SRFLX = 100,
  ICE_TYPE_PREFERENCE_PRFLX = 110,
  ICE_TYPE_PREFERENCE_HOST = 126
};

enum class MdnsNameRegistrationStatus {
  // IP concealment with mDNS is not enabled or the name registration process is
  // not started yet.
  kNotStarted,
  // A request to create and register an mDNS name for a local IP address of a
  // host candidate is sent to the mDNS responder.
  kInProgress,
  // The name registration is complete and the created name is returned by the
  // mDNS responder.
  kCompleted,
};

// Stats that we can return about the port of a STUN candidate.
class StunStats {
 public:
  StunStats() = default;
  StunStats(const StunStats&) = default;
  ~StunStats() = default;

  StunStats& operator=(const StunStats& other) = default;

  int stun_binding_requests_sent = 0;
  int stun_binding_responses_received = 0;
  double stun_binding_rtt_ms_total = 0;
  double stun_binding_rtt_ms_squared_total = 0;
};

// Stats that we can return about a candidate.
class CandidateStats {
 public:
  CandidateStats() = default;
  CandidateStats(const CandidateStats&) = default;
  CandidateStats(CandidateStats&&) = default;
  CandidateStats(Candidate candidate,
                 absl::optional<StunStats> stats = absl::nullopt)
      : candidate_(std::move(candidate)), stun_stats_(std::move(stats)) {}
  ~CandidateStats() = default;

  CandidateStats& operator=(const CandidateStats& other) = default;

  const Candidate& candidate() const { return candidate_; }

  const absl::optional<StunStats>& stun_stats() const { return stun_stats_; }

 private:
  Candidate candidate_;
  // STUN port stats if this candidate is a STUN candidate.
  absl::optional<StunStats> stun_stats_;
};

typedef std::vector<CandidateStats> CandidateStatsList;

const char* ProtoToString(ProtocolType proto);
bool StringToProto(const char* value, ProtocolType* proto);

struct ProtocolAddress {
  rtc::SocketAddress address;
  ProtocolType proto;

  ProtocolAddress(const rtc::SocketAddress& a, ProtocolType p)
      : address(a), proto(p) {}

  bool operator==(const ProtocolAddress& o) const {
    return address == o.address && proto == o.proto;
  }
  bool operator!=(const ProtocolAddress& o) const { return !(*this == o); }
};

struct IceCandidateErrorEvent {
  IceCandidateErrorEvent() = default;
  IceCandidateErrorEvent(std::string address,
                         int port,
                         std::string url,
                         int error_code,
                         std::string error_text)
      : address(std::move(address)),
        port(port),
        url(std::move(url)),
        error_code(error_code),
        error_text(std::move(error_text)) {}

  std::string address;
  int port = 0;
  std::string url;
  int error_code = 0;
  std::string error_text;
};

typedef std::set<rtc::SocketAddress> ServerAddresses;

// Defines the interface for a port, which represents a local communication
// mechanism that can be used to create connections to similar mechanisms of
// the other client. Various types of ports will implement this interface.
class PortInterface {
 public:
  virtual ~PortInterface();

  virtual const std::string& Type() const = 0;
  virtual rtc::Network* Network() const = 0;

  // Methods to set/get ICE role and tiebreaker values.
  virtual void SetIceRole(IceRole role) = 0;
  virtual IceRole GetIceRole() const = 0;

  virtual void SetIceTiebreaker(uint64_t tiebreaker) = 0;
  virtual uint64_t IceTiebreaker() const = 0;

  virtual bool SharedSocket() const = 0;

  // Should not destroy the port even if no connection is using it. Called when
  // a port is ready to use.
  virtual void KeepAliveUntilPruned() = 0;

  // Allows a port to be destroyed if no connection is using it.
  virtual void Prune() = 0;

  virtual bool SupportsProtocol(const std::string& protocol) const = 0;

  // PrepareAddress will attempt to get an address for this port that other
  // clients can send to.  It may take some time before the address is ready.
  // Once it is ready, we will send SignalAddressReady.  If errors are
  // preventing the port from getting an address, it may send
  // SignalAddressError.
  virtual void PrepareAddress() = 0;

  // Returns the connection to the given address or NULL if none exists.
  virtual ConnectionInterface* GetConnection(const rtc::SocketAddress& remote_addr) = 0;

  // Creates a new connection to the given address.
  enum CandidateOrigin { ORIGIN_THIS_PORT, ORIGIN_OTHER_PORT, ORIGIN_MESSAGE };
  virtual ConnectionInterface* CreateConnection(const Candidate& remote_candidate,
                                       CandidateOrigin origin) = 0;

  // Functions on the underlying socket(s).
  virtual int SetOption(rtc::Socket::Option opt, int value) = 0;
  virtual int GetOption(rtc::Socket::Option opt, int* value) = 0;
  virtual int GetError() = 0;

  virtual ProtocolType GetProtocol() const = 0;

  virtual const std::vector<Candidate>& Candidates() const = 0;

  // Sends the given packet to the given address, provided that the address is
  // that of a connection or an address that has sent to us already.
  virtual int SendTo(const void* data,
                     size_t size,
                     const rtc::SocketAddress& addr,
                     const rtc::PacketOptions& options,
                     bool payload) = 0;

  // Indicates that we received a successful STUN binding request from an
  // address that doesn't correspond to any current connection.  To turn this
  // into a real connection, call CreateConnection.
  sigslot::signal6<PortInterface*,
                   const rtc::SocketAddress&,
                   ProtocolType,
                   IceMessage*,
                   const std::string&,
                   bool>
      SignalUnknownAddress;

  // Sends a response message (normal or error) to the given request.  One of
  // these methods should be called as a response to SignalUnknownAddress.
  virtual void SendBindingErrorResponse(StunMessage* request,
                                        const rtc::SocketAddress& addr,
                                        int error_code,
                                        const std::string& reason) = 0;

  // Signaled when this port decides to delete itself because it no longer has
  // any usefulness.
  virtual void SubscribePortDestroyed(
      std::function<void(PortInterface*)> callback) = 0;

  // Returns a map containing all of the connections of this port, keyed by the
  // remote address.
  typedef std::map<rtc::SocketAddress, ConnectionInterface*> AddressMap;
  virtual const AddressMap& connections() = 0;

  // Signaled when Port discovers ice role conflict with the peer.
  sigslot::signal1<PortInterface*> SignalRoleConflict;

  // Normally, packets arrive through a connection (or they result signaling of
  // unknown address).  Calling this method turns off delivery of packets
  // through their respective connection and instead delivers every packet
  // through this port.
  virtual void EnablePortPackets() = 0;
  sigslot::
      signal4<PortInterface*, const char*, size_t, const rtc::SocketAddress&>
          SignalReadPacket;

  // Emitted each time a packet is sent on this port.
  sigslot::signal1<const rtc::SentPacket&> SignalSentPacket;

  virtual std::string ToString() const = 0;

  virtual void GetStunStats(absl::optional<StunStats>* stats) = 0;

  // The thread on which this port performs its I/O.
  virtual rtc::Thread* thread() = 0;

  // The factory used to create the sockets of this port.
  virtual rtc::PacketSocketFactory* socket_factory() const = 0;

  // For debugging purposes.
  virtual const std::string& content_name() const = 0;
  virtual void set_content_name(const std::string& content_name) = 0;

  virtual int component() const = 0;
  virtual void set_component(int component) = 0;

  virtual bool send_retransmit_count_attribute() const = 0;
  virtual void set_send_retransmit_count_attribute(bool enable) = 0;

  // Identifies the generation that this port was created in.
  virtual uint32_t generation() const = 0;
  virtual void set_generation(uint32_t generation) = 0;

  virtual const std::string username_fragment() const = 0;
  virtual const std::string& password() const = 0;

  // May be called when this port was initially created by a pooled
  // PortAllocatorSession, and is now being assigned to an ICE transport.
  // Updates the information for candidates as well.
  virtual void SetIceParameters(int component,
                        const std::string& username_fragment,
                        const std::string& password) = 0;

  // Fired when candidates are discovered by the port. When all candidates
  // are discovered that belong to port SignalAddressReady is fired.
  sigslot::signal2<PortInterface*, const Candidate&> SignalCandidateReady;

  // Fired when candidate discovery failed using certain server.
  sigslot::signal2<PortInterface*, const IceCandidateErrorEvent&> SignalCandidateError;

  // SignalPortComplete is sent when port completes the task of candidates
  // allocation.
  sigslot::signal1<PortInterface*> SignalPortComplete;
  // This signal sent when port fails to allocate candidates and this port
  // can't be used in establishing the connections. When port is in shared mode
  // and port fails to allocate one of the candidates, port shouldn't send
  // this signal as other candidates might be usefull in establishing the
  // connection.
  sigslot::signal1<PortInterface*> SignalPortError;

  // Called each time a connection is created.
  sigslot::signal2<PortInterface*, ConnectionInterface*> SignalConnectionCreated;

  // In a shared socket mode each port which shares the socket will decide
  // to accept the packet based on the `remote_addr`. Currently only UDP
  // port implemented this method.
  // TODO(mallinath) - Make it pure virtual.
  virtual bool HandleIncomingPacket(rtc::AsyncPacketSocket* socket,
                                    const char* data,
                                    size_t size,
                                    const rtc::SocketAddress& remote_addr,
                                    int64_t packet_time_us) = 0;

  // Shall the port handle packet from this `remote_addr`.
  // This method is overridden by TurnPort.
  virtual bool CanHandleIncomingPacketsFrom(
      const rtc::SocketAddress& remote_addr) const = 0;

  virtual void SendUnknownAttributesErrorResponse(
      StunMessage* request,
      const rtc::SocketAddress& addr,
      const std::vector<uint16_t>& unknown_types) = 0;

  virtual void set_proxy(const std::string& user_agent, const rtc::ProxyInfo& proxy)  = 0;
  virtual const std::string& user_agent() = 0;
  virtual const rtc::ProxyInfo& proxy() = 0;

  // Called if the port has no connections and is no longer useful.
  virtual void Destroy() = 0;

  virtual void OnMessage(rtc::Message* pmsg) = 0;

  virtual uint16_t min_port() = 0;
  virtual uint16_t max_port() = 0;

  // Timeout shortening function to speed up unit tests.
  virtual void set_timeout_delay(int delay) = 0;

  // This method will return local and remote username fragements from the
  // stun username attribute if present.
  virtual bool ParseStunUsername(const StunMessage* stun_msg,
                         std::string* local_username,
                         std::string* remote_username) const = 0;
  virtual void CreateStunUsername(const std::string& remote_username,
                          std::string* stun_username_attr_str) const = 0;

  virtual bool MaybeIceRoleConflict(const rtc::SocketAddress& addr,
                            IceMessage* stun_msg,
                            const std::string& remote_ufrag) = 0;

  // Called when the socket is currently able to send.
  virtual void OnReadyToSend() = 0;

  // Called when the Connection discovers a local peer reflexive candidate.
  // Returns the index of the new local candidate.
  virtual size_t AddPrflxCandidate(const Candidate& local) = 0;

  virtual int16_t network_cost() const = 0;

  // If the given data comprises a complete and correct STUN message then the
  // return value is true, otherwise false. If the message username corresponds
  // with this port's username fragment, msg will contain the parsed STUN
  // message.  Otherwise, the function may send a STUN response internally.
  // remote_username contains the remote fragment of the STUN username.
  virtual bool GetStunMessage(const char* data,
                      size_t size,
                      const rtc::SocketAddress& addr,
                      std::unique_ptr<IceMessage>* out_msg,
                      std::string* out_username) = 0;

  // Returns DSCP value packets generated by the port itself should use.
  virtual rtc::DiffServCodePoint StunDscpValue() const = 0;

 protected:
  PortInterface();
};

}  // namespace cricket

#endif  // P2P_BASE_PORT_INTERFACE_H_
