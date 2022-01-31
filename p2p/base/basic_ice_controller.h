/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_BASIC_ICE_CONTROLLER_H_
#define P2P_BASE_BASIC_ICE_CONTROLLER_H_

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "p2p/base/ice_controller_factory_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/p2p_transport_channel.h"

namespace cricket {

class BasicIceController : public IceControllerInterface {
 public:
  explicit BasicIceController(const IceControllerFactoryArgs& args);
  virtual ~BasicIceController();

  void SetIceConfig(const IceConfig& config) override;
  void SetSelectedConnection(const ConnectionInterface* selected_connection) override;
  void AddConnection(const ConnectionInterface* connection) override;
  void OnConnectionDestroyed(const ConnectionInterface* connection) override;
  rtc::ArrayView<const ConnectionInterface*> connections() const override {
    return rtc::ArrayView<const ConnectionInterface*>(
        const_cast<const ConnectionInterface**>(connections_.data()),
        connections_.size());
  }

  bool HasPingableConnection() const override;

  PingResult SelectConnectionToPing(int64_t last_ping_sent_ms) override;

  bool GetUseCandidateAttr(const ConnectionInterface* conn,
                           NominationMode mode,
                           IceMode remote_ice_mode) const override;

  SwitchResult ShouldSwitchConnection(IceControllerEvent reason,
                                      const ConnectionInterface* connection) override;
  SwitchResult SortAndSwitchConnection(IceControllerEvent reason) override;

  std::vector<const ConnectionInterface*> PruneConnections() override;

  // These methods are only for tests.
  const ConnectionInterface* FindNextPingableConnection() override;
  void MarkConnectionPinged(const ConnectionInterface* conn) override;

 private:
  // A transport channel is weak if the current best connection is either
  // not receiving or not writable, or if there is no best connection at all.
  bool weak() const {
    return !selected_connection_ || selected_connection_->weak();
  }

  int weak_ping_interval() const {
    return std::max(config_.ice_check_interval_weak_connectivity_or_default(),
                    config_.ice_check_min_interval_or_default());
  }

  int strong_ping_interval() const {
    return std::max(config_.ice_check_interval_strong_connectivity_or_default(),
                    config_.ice_check_min_interval_or_default());
  }

  int check_receiving_interval() const {
    return std::max(MIN_CHECK_RECEIVING_INTERVAL,
                    config_.receiving_timeout_or_default() / 10);
  }

  const ConnectionInterface* FindOldestConnectionNeedingTriggeredCheck(int64_t now);
  // Between `conn1` and `conn2`, this function returns the one which should
  // be pinged first.
  const ConnectionInterface* MorePingable(const ConnectionInterface* conn1,
                                 const ConnectionInterface* conn2);
  // Select the connection which is Relay/Relay. If both of them are,
  // UDP relay protocol takes precedence.
  const ConnectionInterface* MostLikelyToWork(const ConnectionInterface* conn1,
                                     const ConnectionInterface* conn2);
  // Compare the last_ping_sent time and return the one least recently pinged.
  const ConnectionInterface* LeastRecentlyPinged(const ConnectionInterface* conn1,
                                        const ConnectionInterface* conn2);

  bool IsPingable(const ConnectionInterface* conn, int64_t now) const;
  bool IsBackupConnection(const ConnectionInterface* conn) const;
  // Whether a writable connection is past its ping interval and needs to be
  // pinged again.
  bool WritableConnectionPastPingInterval(const ConnectionInterface* conn,
                                          int64_t now) const;
  int CalculateActiveWritablePingInterval(const ConnectionInterface* conn,
                                          int64_t now) const;

  std::map<const rtc::Network*, const ConnectionInterface*> GetBestConnectionByNetwork()
      const;
  std::vector<const ConnectionInterface*> GetBestWritableConnectionPerNetwork() const;

  bool ReadyToSend(const ConnectionInterface* connection) const;
  bool PresumedWritable(const ConnectionInterface* conn) const;

  int CompareCandidatePairNetworks(
      const ConnectionInterface* a,
      const ConnectionInterface* b,
      absl::optional<rtc::AdapterType> network_preference) const;

  // The methods below return a positive value if `a` is preferable to `b`,
  // a negative value if `b` is preferable, and 0 if they're equally preferable.
  // If `receiving_unchanged_threshold` is set, then when `b` is receiving and
  // `a` is not, returns a negative value only if `b` has been in receiving
  // state and `a` has been in not receiving state since
  // `receiving_unchanged_threshold` and sets
  // `missed_receiving_unchanged_threshold` to true otherwise.
  int CompareConnectionStates(
      const ConnectionInterface* a,
      const ConnectionInterface* b,
      absl::optional<int64_t> receiving_unchanged_threshold,
      bool* missed_receiving_unchanged_threshold) const;
  int CompareConnectionCandidates(const ConnectionInterface* a,
                                  const ConnectionInterface* b) const;
  // Compares two connections based on the connection states
  // (writable/receiving/connected), nomination states, last data received time,
  // and static preferences. Does not include latency. Used by both sorting
  // and ShouldSwitchSelectedConnection().
  // Returns a positive value if `a` is better than `b`.
  int CompareConnections(const ConnectionInterface* a,
                         const ConnectionInterface* b,
                         absl::optional<int64_t> receiving_unchanged_threshold,
                         bool* missed_receiving_unchanged_threshold) const;

  SwitchResult HandleInitialSelectDampening(IceControllerEvent reason,
                                            const ConnectionInterface* new_connection);

  std::function<IceTransportState()> ice_transport_state_func_;
  std::function<IceRole()> ice_role_func_;
  std::function<bool(const ConnectionInterface*)> is_connection_pruned_func_;

  IceConfig config_;
  const IceFieldTrials* field_trials_;

  // `connections_` is a sorted list with the first one always be the
  // `selected_connection_` when it's not nullptr. The combination of
  // `pinged_connections_` and `unpinged_connections_` has the same
  // connections as `connections_`. These 2 sets maintain whether a
  // connection should be pinged next or not.
  const ConnectionInterface* selected_connection_ = nullptr;
  std::vector<const ConnectionInterface*> connections_;
  std::set<const ConnectionInterface*> pinged_connections_;
  std::set<const ConnectionInterface*> unpinged_connections_;

  // Timestamp for when we got the first selectable connection.
  int64_t initial_select_timestamp_ms_ = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_BASIC_ICE_CONTROLLER_H_
