/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_CONNECTION_INTERFACE_H_
#define P2P_BASE_CONNECTION_INTERFACE_H_

#include "p2p/base/candidate_pair_interface.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/p2p_transport_channel_ice_field_trials.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/numerics/event_based_exponential_moving_average.h"

namespace cricket {

// Connection and Port has circular dependencies.
// So we use forward declaration rather than include.
class Port;

class ConnectionInterface : public CandidatePairInterface {
 public:
  struct SentPing {
    SentPing(const std::string id, int64_t sent_time, uint32_t nomination)
        : id(id), sent_time(sent_time), nomination(nomination) {}

    std::string id;
    int64_t sent_time;
    uint32_t nomination;
  };

  virtual ~ConnectionInterface() override = default;

  // A unique ID assigned when the connection is created.
  virtual uint32_t id() const = 0;
  // Return local network for this connection.
  virtual const rtc::Network* network() const = 0;
  // Return generation for this connection.
  virtual int generation() const = 0;
  // Returns the pair priority.
  virtual uint64_t priority() const = 0;

  enum WriteState {
    STATE_WRITABLE = 0,          // we have received ping responses recently
    STATE_WRITE_UNRELIABLE = 1,  // we have had a few ping failures
    STATE_WRITE_INIT = 2,        // we have yet to receive a ping response
    STATE_WRITE_TIMEOUT = 3,     // we have had a large number of ping failures
  };

  virtual WriteState write_state() const = 0;
  virtual bool writable() const = 0;
  virtual bool receiving() const = 0;

  // Determines whether the connection has finished connecting.  This can only
  // be false for TCP connections.
  virtual bool connected() const = 0;
  virtual bool weak() const = 0;
  virtual bool active() const = 0;

  // A connection is dead if it can be safely deleted.
  virtual bool dead(int64_t now) const = 0;

  // Estimate of the round-trip time over this connection.
  virtual int rtt() const = 0;

  virtual int unwritable_timeout() const = 0;
  virtual void set_unwritable_timeout(const absl::optional<int>& value_ms) = 0;
  virtual int unwritable_min_checks() const = 0;
  virtual void set_unwritable_min_checks(const absl::optional<int>& value) = 0;
  virtual int inactive_timeout() const = 0;
  virtual void set_inactive_timeout(const absl::optional<int>& value) = 0;

  // Gets the `ConnectionInfo` stats, where `best_connection` has not been
  // populated (default value false).
  virtual ConnectionInfo stats() = 0;

  sigslot::signal1<ConnectionInterface*> SignalStateChange;

  // Sent when the connection has decided that it is no longer of value.  It
  // will delete itself immediately after this call.
  sigslot::signal1<ConnectionInterface*> SignalDestroyed;

  // The connection can send and receive packets asynchronously.  This matches
  // the interface of AsyncPacketSocket, which may use UDP or TCP under the
  // covers.
  virtual int Send(const void* data,
                   size_t size,
                   const rtc::PacketOptions& options) = 0;

  // Error if Send() returns < 0
  virtual int GetError() = 0;

  sigslot::signal4<ConnectionInterface*, const char*, size_t, int64_t> SignalReadPacket;

  sigslot::signal1<ConnectionInterface*> SignalReadyToSend;

  // Called when a packet is received on this connection.
  virtual void OnReadPacket(const char* data, size_t size, int64_t packet_time_us) = 0;

  // Called when the socket is currently able to send.
  virtual void OnReadyToSend() = 0;

  // Called when a connection is determined to be no longer useful to us.  We
  // still keep it around in case the other side wants to use it.  But we can
  // safely stop pinging on it and we can allow it to time out if the other
  // side stops using it as well.
  virtual bool pruned() const = 0;
  virtual void Prune() = 0;

  virtual bool use_candidate_attr() const = 0;
  virtual void set_use_candidate_attr(bool enable) = 0;

  virtual void set_nomination(uint32_t value) = 0;

  virtual uint32_t remote_nomination() const = 0;
  // One or several pairs may be nominated based on if Regular or Aggressive
  // Nomination is used. https://tools.ietf.org/html/rfc5245#section-8
  // `nominated` is defined both for the controlling or controlled agent based
  // on if a nomination has been pinged or acknowledged. The controlled agent
  // gets its `remote_nomination_` set when pinged by the controlling agent with
  // a nomination value. The controlling agent gets its `acked_nomination_` set
  // when receiving a response to a nominating ping.
  virtual bool nominated() const = 0;
  virtual void set_remote_ice_mode(IceMode mode) = 0;

  virtual int receiving_timeout() const = 0;
  virtual void set_receiving_timeout(absl::optional<int> receiving_timeout_ms) = 0;

  // Makes the connection go away.
  virtual void Destroy() = 0;

  // Makes the connection go away, in a failed state.
  virtual void FailAndDestroy() = 0;

  // Prunes the connection and sets its state to STATE_FAILED,
  // It will not be used or send pings although it can still receive packets.
  virtual void FailAndPrune() = 0;

  // Checks that the state of this connection is up-to-date.  The argument is
  // the current time, which is compared against various timeouts.
  virtual void UpdateState(int64_t now) = 0;

  // Called when this connection should try checking writability again.
  virtual int64_t last_ping_sent() const = 0;
  virtual void Ping(int64_t now) = 0;
  virtual void ReceivedPingResponse(
      int rtt,
      const std::string& request_id,
      const absl::optional<uint32_t>& nomination = absl::nullopt) = 0;
  virtual int64_t last_ping_response_received() const = 0;
  virtual const absl::optional<std::string>& last_ping_id_received() const = 0;
  // Used to check if any STUN ping response has been received.
  virtual int rtt_samples() const = 0;

  // Called whenever a valid ping is received on this connection.  This is
  // public because the connection intercepts the first ping for us.
  virtual int64_t last_ping_received() const = 0;
  virtual void ReceivedPing(
      const absl::optional<std::string>& request_id = absl::nullopt) = 0;
  // Handles the binding request; sends a response if this is a valid request.
  virtual void HandleStunBindingOrGoogPingRequest(IceMessage* msg) = 0;
  // Handles the piggyback acknowledgement of the lastest connectivity check
  // that the remote peer has received, if it is indicated in the incoming
  // connectivity check from the peer.
  virtual void HandlePiggybackCheckAcknowledgementIfAny(StunMessage* msg) = 0;
  // Timestamp when data was last sent (or attempted to be sent).
  virtual int64_t last_send_data() const = 0;
  virtual int64_t last_data_received() const = 0;

  // Debugging description of this connection
  virtual std::string ToDebugId() const = 0;
  virtual std::string ToString() const = 0;
  virtual std::string ToSensitiveString() const = 0;
  // Structured description of this candidate pair.
  virtual const webrtc::IceCandidatePairDescription& ToLogDescription() = 0;
  virtual void set_ice_event_log(webrtc::IceEventLog* ice_event_log) = 0;
  // Prints pings_since_last_response_ into a string.
  virtual void PrintPingsSinceLastResponse(std::string* pings, size_t max) = 0;

  virtual bool reported() const = 0;
  virtual void set_reported(bool reported) = 0;
  // The following two methods are only used for logging in ToString above, and
  // this flag is set true by P2PTransportChannel for its selected candidate
  // pair.
  virtual bool selected() const = 0;
  virtual void set_selected(bool selected) = 0;

  // This signal will be fired if this connection is nominated by the
  // controlling side.
  sigslot::signal1<ConnectionInterface*> SignalNominated;

  // Invoked when Connection receives STUN error response with 487 code.
  virtual void HandleRoleConflictFromPeer() = 0;

  virtual IceCandidatePairState state() const = 0;

  virtual int num_pings_sent() const = 0;

  virtual IceMode remote_ice_mode() const = 0;

  virtual uint32_t ComputeNetworkCost() const = 0;

  // Update the ICE password and/or generation of the remote candidate if the
  // ufrag in `params` matches the candidate's ufrag, and the
  // candidate's password and/or ufrag has not been set.
  virtual void MaybeSetRemoteIceParametersAndGeneration(const IceParameters& params,
                                                int generation) = 0;

  // If `remote_candidate_` is peer reflexive and is equivalent to
  // `new_candidate` except the type, update `remote_candidate_` to
  // `new_candidate`.
  virtual void MaybeUpdatePeerReflexiveCandidate(const Candidate& new_candidate) = 0;

  // Returns the last received time of any data, stun request, or stun
  // response in milliseconds
  virtual int64_t last_received() const = 0;
  // Returns the last time when the connection changed its receiving state.
  virtual int64_t receiving_unchanged_since() const = 0;

  virtual bool stable(int64_t now) const = 0;

  // Check if we sent `val` pings without receving a response.
  virtual bool TooManyOutstandingPings(const absl::optional<int>& val) const = 0;

  virtual void SetIceFieldTrials(const IceFieldTrials* field_trials) = 0;
  virtual const rtc::EventBasedExponentialMovingAverage& GetRttEstimate() const = 0;

  // Reset the connection to a state of a newly connected.
  // - STATE_WRITE_INIT
  // - receving = false
  // - throw away all pending request
  // - reset RttEstimate
  //
  // Keep the following unchanged:
  // - connected
  // - remote_candidate
  // - statistics
  //
  // Does not trigger SignalStateChange
  virtual void ForgetLearnedState() = 0;

  virtual void SendStunBindingResponse(const StunMessage* request) = 0;
  virtual void SendGoogPingResponse(const StunMessage* request) = 0;
  virtual void SendResponseMessage(const StunMessage& response) = 0;

  // An accessor for unit tests.
  virtual Port* PortForTest() = 0;
  virtual const Port* PortForTest() const = 0;

  // Public for unit tests.
  virtual uint32_t acked_nomination() const = 0;

  // Public for unit tests.
  virtual void set_remote_nomination(uint32_t remote_nomination) = 0;

 protected:
  // The local port where this connection sends and receives packets.
  virtual Port* port() = 0;
  virtual const Port* port() const = 0;

  friend class Port;
  friend class P2PTransportChannel;
};

}  // namespace cricket

#endif  // P2P_BASE_PORT_INTERFACE_H_
