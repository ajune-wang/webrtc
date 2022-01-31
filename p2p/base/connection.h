/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_CONNECTION_H_
#define P2P_BASE_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/transport/stun.h"
#include "logging/rtc_event_log/ice_logger.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/connection_interface.h"
#include "p2p/base/stun_request.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/network.h"
#include "rtc_base/rate_tracker.h"

namespace cricket {

// Version number for GOOG_PING, this is added to have the option of
// adding other flavors in the future.
constexpr int kGoogPingVersion = 1;

// Forward declaration so that a ConnectionRequest can contain a Connection.
class Connection;

struct CandidatePair final : public CandidatePairInterface {
  ~CandidatePair() override = default;

  const Candidate& local_candidate() const override { return local; }
  const Candidate& remote_candidate() const override { return remote; }

  Candidate local;
  Candidate remote;
};

// A ConnectionRequest is a simple STUN ping used to determine writability.
class ConnectionRequest : public StunRequest {
 public:
  explicit ConnectionRequest(Connection* connection);
  void Prepare(StunMessage* request) override;
  void OnResponse(StunMessage* response) override;
  void OnErrorResponse(StunMessage* response) override;
  void OnTimeout() override;
  void OnSent() override;
  int resend_delay() override;

 private:
  Connection* const connection_;
};

// Represents a communication link between a port on the local client and a
// port on the remote client.
class Connection : public ConnectionInterface,
                   public rtc::MessageHandlerAutoCleanup,
                   public sigslot::has_slots<> {
 public:
  ~Connection() override = default;

  uint32_t id() const override { return id_; }

  // Implementation of virtual methods in CandidatePairInterface.
  // Returns the description of the local port
  const Candidate& local_candidate() const override;
  // Returns the description of the remote port to which we communicate.
  const Candidate& remote_candidate() const override;

  virtual const rtc::Network* network() const override;
  virtual int generation() const override;
  virtual uint64_t priority() const override;
  WriteState write_state() const override { return write_state_; }
  bool writable() const override { return write_state_ == STATE_WRITABLE; }
  bool receiving() const override { return receiving_; }
  bool connected() const override { return connected_; }
  bool weak() const override { return !(writable() && receiving() && connected()); }
  bool active() const override { return write_state_ != STATE_WRITE_TIMEOUT; }
  bool dead(int64_t now) const override;

  // Estimate of the round-trip time over this connection.
  int rtt() const override { return rtt_; }

  int unwritable_timeout() const override;
  void set_unwritable_timeout(const absl::optional<int>& value_ms) override {
    unwritable_timeout_ = value_ms;
  }
  int unwritable_min_checks() const override;
  void set_unwritable_min_checks(const absl::optional<int>& value) override {
    unwritable_min_checks_ = value;
  }
  int inactive_timeout() const override;
  void set_inactive_timeout(const absl::optional<int>& value) override {
    inactive_timeout_ = value;
  }

  ConnectionInfo stats() override;
  virtual int Send(const void* data,
                   size_t size,
                   const rtc::PacketOptions& options) override = 0;
  virtual int GetError() override = 0;
  void OnReadPacket(const char* data, size_t size, int64_t packet_time_us) override;
  void OnReadyToSend() override;
  bool pruned() const override { return pruned_; }
  void Prune() override;
  bool use_candidate_attr() const override { return use_candidate_attr_; }
  void set_use_candidate_attr(bool enable) override;
  void set_nomination(uint32_t value) override { nomination_ = value; }
  uint32_t remote_nomination() const override { return remote_nomination_; }
  bool nominated() const override { return acked_nomination_ || remote_nomination_; }
  void set_remote_ice_mode(IceMode mode) override { remote_ice_mode_ = mode; }
  int receiving_timeout() const override;
  void set_receiving_timeout(absl::optional<int> receiving_timeout_ms) override {
    receiving_timeout_ = receiving_timeout_ms;
  }
  void Destroy() override;
  void FailAndDestroy() override;
  void FailAndPrune() override;
  void UpdateState(int64_t now) override;

  int64_t last_ping_sent() const override { return last_ping_sent_; }
  void Ping(int64_t now) override;
  void ReceivedPingResponse(
      int rtt,
      const std::string& request_id,
      const absl::optional<uint32_t>& nomination = absl::nullopt) override;
  int64_t last_ping_response_received() const override {
    return last_ping_response_received_;
  }
  const absl::optional<std::string>& last_ping_id_received() const override {
    return last_ping_id_received_;
  }
  int rtt_samples() const override { return rtt_samples_; }
  int64_t last_ping_received() const override { return last_ping_received_; }
  void ReceivedPing(
      const absl::optional<std::string>& request_id = absl::nullopt) override;
  void HandleStunBindingOrGoogPingRequest(IceMessage* msg) override;
  void HandlePiggybackCheckAcknowledgementIfAny(StunMessage* msg) override;
  int64_t last_send_data() const override { return last_send_data_; }
  int64_t last_data_received() const override { return last_data_received_; }

  std::string ToDebugId() const override;
  std::string ToString() const override;
  std::string ToSensitiveString() const override;
  const webrtc::IceCandidatePairDescription& ToLogDescription() override;
  void set_ice_event_log(webrtc::IceEventLog* ice_event_log) override {
    ice_event_log_ = ice_event_log;
  }
  void PrintPingsSinceLastResponse(std::string* pings, size_t max) override;
  bool reported() const override { return reported_; }
  void set_reported(bool reported) override { reported_ = reported; }
  bool selected() const override { return selected_; }
  void set_selected(bool selected) override { selected_ = selected; }
  void HandleRoleConflictFromPeer() override;
  IceCandidatePairState state() const override { return state_; }
  int num_pings_sent() const override { return num_pings_sent_; }
  IceMode remote_ice_mode() const override { return remote_ice_mode_; }
  uint32_t ComputeNetworkCost() const override;
  void MaybeSetRemoteIceParametersAndGeneration(const IceParameters& params,
                                                int generation) override;
  void MaybeUpdatePeerReflexiveCandidate(const Candidate& new_candidate) override;
  int64_t last_received() const override;
  int64_t receiving_unchanged_since() const override{
    return receiving_unchanged_since_;
  }
  bool stable(int64_t now) const override;
  bool TooManyOutstandingPings(const absl::optional<int>& val) const override;
  void SetIceFieldTrials(const IceFieldTrials* field_trials) override;
  const rtc::EventBasedExponentialMovingAverage& GetRttEstimate() const override {
    return rtt_estimate_;
  }
  void ForgetLearnedState() override;
  void SendStunBindingResponse(const StunMessage* request) override;
  void SendGoogPingResponse(const StunMessage* request) override;
  void SendResponseMessage(const StunMessage& response) override;
  Port* PortForTest() override { return port_; }
  const Port* PortForTest() const override { return port_; }
  uint32_t acked_nomination() const override { return acked_nomination_; }
  void set_remote_nomination(uint32_t remote_nomination) override {
    remote_nomination_ = remote_nomination;
  }

 protected:
  enum { MSG_DELETE = 0, MSG_FIRST_AVAILABLE };

  // Constructs a new connection to the given remote port.
  Connection(Port* port, size_t index, const Candidate& candidate);

  // Called back when StunRequestManager has a stun packet to send
  void OnSendStunPacket(const void* data, size_t size, StunRequest* req);

  // Callbacks from ConnectionRequest
  virtual void OnConnectionRequestResponse(ConnectionRequest* req,
                                           StunMessage* response);
  void OnConnectionRequestErrorResponse(ConnectionRequest* req,
                                        StunMessage* response);
  void OnConnectionRequestTimeout(ConnectionRequest* req);
  void OnConnectionRequestSent(ConnectionRequest* req);

  bool rtt_converged() const;

  // If the response is not received within 2 * RTT, the response is assumed to
  // be missing.
  bool missing_responses(int64_t now) const;

  // Changes the state and signals if necessary.
  void set_write_state(WriteState value);
  void UpdateReceiving(int64_t now);
  void set_state(IceCandidatePairState state);
  void set_connected(bool value);

  uint32_t nomination() const { return nomination_; }

  void OnMessage(rtc::Message* pmsg) override;

  Port* port() override { return port_; }
  const Port* port() const override { return port_; }

  uint32_t id_;
  Port* port_;
  size_t local_candidate_index_;
  Candidate remote_candidate_;

  ConnectionInfo stats_;
  rtc::RateTracker recv_rate_tracker_;
  rtc::RateTracker send_rate_tracker_;
  int64_t last_send_data_ = 0;

 private:
  // Update the local candidate based on the mapped address attribute.
  // If the local candidate changed, fires SignalStateChange.
  void MaybeUpdateLocalCandidate(ConnectionRequest* request,
                                 StunMessage* response);

  void LogCandidatePairConfig(webrtc::IceCandidatePairConfigType type);
  void LogCandidatePairEvent(webrtc::IceCandidatePairEventType type,
                             uint32_t transaction_id);

  // Check if this IceMessage is identical
  // to last message ack:ed STUN_BINDING_REQUEST.
  bool ShouldSendGoogPing(const StunMessage* message);

  WriteState write_state_;
  bool receiving_;
  bool connected_;
  bool pruned_;
  bool selected_ = false;
  // By default `use_candidate_attr_` flag will be true,
  // as we will be using aggressive nomination.
  // But when peer is ice-lite, this flag "must" be initialized to false and
  // turn on when connection becomes "best connection".
  bool use_candidate_attr_;
  // Used by the controlling side to indicate that this connection will be
  // selected for transmission if the peer supports ICE-renomination when this
  // value is positive. A larger-value indicates that a connection is nominated
  // later and should be selected by the controlled side with higher precedence.
  // A zero-value indicates not nominating this connection.
  uint32_t nomination_ = 0;
  // The last nomination that has been acknowledged.
  uint32_t acked_nomination_ = 0;
  // Used by the controlled side to remember the nomination value received from
  // the controlling side. When the peer does not support ICE re-nomination, its
  // value will be 1 if the connection has been nominated.
  uint32_t remote_nomination_ = 0;

  IceMode remote_ice_mode_;
  StunRequestManager requests_;
  int rtt_;
  int rtt_samples_ = 0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-totalroundtriptime
  uint64_t total_round_trip_time_ms_ = 0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-currentroundtriptime
  absl::optional<uint32_t> current_round_trip_time_ms_;
  int64_t last_ping_sent_;      // last time we sent a ping to the other side
  int64_t last_ping_received_;  // last time we received a ping from the other
                                // side
  int64_t last_data_received_;
  int64_t last_ping_response_received_;
  int64_t receiving_unchanged_since_ = 0;
  std::vector<SentPing> pings_since_last_response_;
  // Transaction ID of the last connectivity check received. Null if having not
  // received a ping yet.
  absl::optional<std::string> last_ping_id_received_;

  absl::optional<int> unwritable_timeout_;
  absl::optional<int> unwritable_min_checks_;
  absl::optional<int> inactive_timeout_;

  bool reported_;
  IceCandidatePairState state_;
  // Time duration to switch from receiving to not receiving.
  absl::optional<int> receiving_timeout_;
  int64_t time_created_ms_;
  int num_pings_sent_ = 0;

  absl::optional<webrtc::IceCandidatePairDescription> log_description_;
  webrtc::IceEventLog* ice_event_log_ = nullptr;

  // GOOG_PING_REQUEST is sent in place of STUN_BINDING_REQUEST
  // if configured via field trial, the remote peer supports it (signaled
  // in STUN_BINDING) and if the last STUN BINDING is identical to the one
  // that is about to be sent.
  absl::optional<bool> remote_support_goog_ping_;
  std::unique_ptr<StunMessage> cached_stun_binding_;

  const IceFieldTrials* field_trials_;
  rtc::EventBasedExponentialMovingAverage rtt_estimate_;

  friend class ConnectionRequest;
};

// ProxyConnection defers all the interesting work to the port.
class ProxyConnection : public Connection {
 public:
  ProxyConnection(Port* port, size_t index, const Candidate& remote_candidate);

  int Send(const void* data,
           size_t size,
           const rtc::PacketOptions& options) override;
  int GetError() override;

 private:
  int error_ = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_CONNECTION_H_
