/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// BBR (Bottleneck Bandwidth and RTT) congestion control algorithm.
// Based on the Quic BBR implementation in Chromium

#ifndef MODULES_CONGESTION_CONTROLLER_BBR_BBR_NETWORK_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_BBR_NETWORK_CONTROLLER_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "modules/congestion_controller/bbr/data_rate_calculator.h"
#include "modules/congestion_controller/bbr/rtt_stats.h"
#include "modules/congestion_controller/bbr/windowed_filter.h"
#include "network_control/include/network_control.h"
#include "network_control/include/network_types.h"
#include "network_control/include/network_units.h"

#include "api/optional.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace network {
namespace bbr {

typedef int64_t BbrPacketCount;
typedef int64_t BbrRoundTripCount;

// BbrSender implements BBR congestion control algorithm.  BBR aims to estimate
// the current available Bottleneck Bandwidth and RTT (hence the name), and
// regulates the pacing rate and the size of the congestion window based on
// those signals.
//
// BBR relies on pacing in order to function properly.  Do not use BBR when
// pacing is disabled.
class BbrNetworkController : public NetworkControllerInterface {
 public:
  enum Mode {
    // Startup phase of the connection.
    STARTUP,
    // After achieving the highest possible bandwidth during the startup, lower
    // the pacing rate in order to drain the queue.
    DRAIN,
    // Cruising mode.
    PROBE_BW,
    // Temporarily slow down sending in order to empty the buffer and measure
    // the real minimum RTT.
    PROBE_RTT,
  };

  // Indicates how the congestion control limits the amount of bytes in flight.
  enum RecoveryState {
    // Do not limit.
    NOT_IN_RECOVERY,
    // Allow an extra outstanding byte for each byte acknowledged.
    CONSERVATION,
    // Allow 1.5 extra outstanding bytes for each byte acknowledged.
    MEDIUM_GROWTH,
    // Allow two extra outstanding bytes for each byte acknowledged (slow
    // start).
    GROWTH
  };

  // Debug state can be exported in order to troubleshoot potential congestion
  // control issues.
  struct DebugState {
    explicit DebugState(const BbrNetworkController& sender);
    DebugState(const DebugState& state);

    Mode mode;
    units::DataRate max_bandwidth;
    BbrRoundTripCount round_trip_count;
    int gain_cycle_index;
    units::DataSize congestion_window;

    bool is_at_full_bandwidth;
    units::DataRate bandwidth_at_last_round;
    BbrRoundTripCount rounds_without_bandwidth_gain;

    units::TimeDelta min_rtt;
    units::Timestamp min_rtt_timestamp;

    RecoveryState recovery_state;
    units::DataSize recovery_window;

    bool last_sample_is_app_limited;
    units::Timestamp end_of_app_limited_phase;
  };

  BbrNetworkController();
  ~BbrNetworkController() override;
  units::TimeDelta GetProcessInterval() override;
  NetworkInformationReceivers GetReceivers() override;
  NetworkControlProducers GetProducers() override;

 private:
  struct BbrControllerConfig {
    static BbrControllerConfig DefaultConfig();
    static BbrControllerConfig QUICConfig();
    static BbrControllerConfig GBBRConfig();
    static BbrControllerConfig ExperimentConfig();

    float probe_bw_pacing_gain_offset;
    float encoder_rate_gain;
    float encoder_rate_gain_in_probe_rtt;
    // RTT delta to determine if startup should be exited due to increased RTT
    int64_t exit_startup_rtt_threshold_ms;

    float probe_rtt_congestion_window_gain;

    // Configurable in QUIC BBR:
    bool exit_startup_on_loss;
    // The number of RTTs to stay in STARTUP mode.  Defaults to 3.
    BbrRoundTripCount num_startup_rtts;
    // When true, recovery is rate based rather than congestion window based.
    bool rate_based_recovery;
    float max_aggregation_bytes_multiplier;
    // When true, pace at 1.5x and disable packet conservation in STARTUP.
    bool slower_startup;
    // When true, disables packet conservation in STARTUP.
    bool rate_based_startup;
    // If true, will not exit low gain mode until bytes_in_flight drops below
    // BDP or it's time for high gain mode.
    bool fully_drain_queue;
    // Used as the initial packet conservation mode when first entering
    // recovery.
    RecoveryState initial_conservation_in_startup;
    //  MEDIUM_GROWTH; GROWTH;
    double max_ack_height_window_multiplier;
    // If true, use a CWND of 0.75*BDP during probe_rtt instead of 4 packets.
    bool probe_rtt_based_on_bdp;
    // If true, skip probe_rtt and update the timestamp of the existing min_rtt
    // to now if min_rtt over the last cycle is within 12.5% of the current
    // min_rtt. Even if the min_rtt is 12.5% too low, the 25% gain cycling and
    // 2x CWND gain should overcome an overly small min_rtt.
    bool probe_rtt_skipped_if_similar_rtt;
    // If true, disable PROBE_RTT entirely as long as the connection was
    // recently app limited.
    bool probe_rtt_disabled_if_app_limited;
  };
  void Reset();
  void SignalUpdatedRates(units::Timestamp at_time);

  bool InSlowStart() const;
  bool InRecovery() const;
  bool IsProbingForMoreBandwidth() const;

  bool CanSend(units::DataSize bytes_in_flight);
  units::DataRate PacingRate() const;
  units::DataRate BandwidthEstimate() const;
  units::DataSize GetCongestionWindow() const;

  float GetPacingGain(int round_offset) const;

  std::string GetDebugState() const;
  void OnApplicationLimited(units::DataSize bytes_in_flight);
  // End implementation of SendAlgorithmInterface.

  DebugState ExportDebugState() const;

  void OnNetworkAvailability(NetworkAvailability);
  void OnNetworkRouteChange(NetworkRouteChange);
  void OnProcessInterval(ProcessInterval);
  void OnRemoteBitrateReport(RemoteBitrateReport);
  void OnRoundTripTimeReport(RoundTripTimeReport);
  void OnSentPacket(SentPacket);
  void OnStreamsConfig(StreamsConfig);
  void OnTargetRateConstraints(TargetRateConstraints);
  void OnTransportLossReport(TransportLossReport);
  void OnTransportPacketsFeedback(TransportPacketsFeedback);

  typedef WindowedFilter<units::DataRate,
                         MaxFilter<units::DataRate>,
                         BbrRoundTripCount,
                         BbrRoundTripCount>
      MaxBandwidthFilter;

  typedef WindowedFilter<units::TimeDelta,
                         MaxFilter<units::TimeDelta>,
                         BbrRoundTripCount,
                         BbrRoundTripCount>
      MaxAckDelayFilter;

  typedef WindowedFilter<units::DataSize,
                         MaxFilter<units::DataSize>,
                         BbrRoundTripCount,
                         BbrRoundTripCount>
      MaxAckHeightFilter;

  // Returns the current estimate of the RTT of the connection.  Outside of the
  // edge cases, this is minimum RTT.
  units::TimeDelta GetMinRtt() const;

  // Computes the target congestion window using the specified gain.
  units::DataSize GetTargetCongestionWindow(float gain) const;
  // The target congestion window during PROBE_RTT.
  units::DataSize ProbeRttCongestionWindow() const;
  // Returns true if the current min_rtt should be kept and we should not enter
  // PROBE_RTT immediately.
  bool ShouldExtendMinRttExpiry() const;

  // Enters the STARTUP mode.
  void EnterStartupMode();
  // Enters the PROBE_BW mode.
  void EnterProbeBandwidthMode(units::Timestamp now);

  // Updates the round-trip counter if a round-trip has passed.  Returns true if
  // the counter has been advanced.
  bool UpdateRoundTripCounter(units::Timestamp last_acked_timestamp);
  // Updates the current bandwidth and min_rtt estimate based on the samples for
  // the received acknowledgements.  Returns true if min_rtt has expired.
  void UpdateBandwidth(units::Timestamp now,
                       const std::vector<NetworkPacketFeedback>& acked_packets);
  bool UpdateMinRtt(units::Timestamp ack_time,
                    units::Timestamp last_packet_send_time);
  // Updates the current gain used in PROBE_BW mode.
  void UpdateGainCyclePhase(units::Timestamp now,
                            units::DataSize prior_in_flight,
                            bool has_losses);
  // Tracks for how many round-trips the bandwidth has not increased
  // significantly.
  void CheckIfFullBandwidthReached();
  // Transitions from STARTUP to DRAIN and from DRAIN to PROBE_BW if
  // appropriate.
  void MaybeExitStartupOrDrain(const TransportPacketsFeedback&);
  // Decides whether to enter or exit PROBE_RTT.
  void MaybeEnterOrExitProbeRtt(const TransportPacketsFeedback& msg,
                                bool is_round_start,
                                bool min_rtt_expired);
  // Determines whether BBR needs to enter, exit or advance state of the
  // recovery.
  void UpdateRecoveryState(units::Timestamp last_acked_send_time,
                           bool has_losses,
                           bool is_round_start);

  // Updates the ack aggregation max filter in bytes.
  void UpdateAckAggregationBytes(units::Timestamp ack_time,
                                 units::DataSize newly_acked_bytes);

  // Determines the appropriate pacing rate for the connection.
  void CalculatePacingRate();
  // Determines the appropriate congestion window for the connection.
  void CalculateCongestionWindow(units::DataSize bytes_acked);
  // Determines the approriate wQuicPacketNumberindow that constrains the
  // in-flight during recovery.
  void CalculateRecoveryWindow(units::DataSize bytes_acked,
                               units::DataSize bytes_lost,
                               units::DataSize bytes_in_flight);

  CongestionWindow::SimpleJunction CongestionWindowJunction;
  PacerConfig::SimpleJunction PacerConfigJunction;
  ProbeClusterConfig::SimpleJunction ProbeClusterConfigJunction;
  TargetTransferRate::SimpleJunction TargetTransferRateJunction;

  RttStats rtt_stats_;
  webrtc::Random random_;

  DataRateCalculator send_ack_tracker_;

  rtc::Optional<TargetRateConstraints> constraints_;

  Mode mode_;

  BbrControllerConfig config_;

  // The total number of congestion controlled bytes which were acknowledged.
  units::DataSize total_bytes_acked_;

  // The total number of congestion controlled bytes sent during the connection.
  units::DataSize total_bytes_sent_;

  // The time at which the last acknowledged packet was sent. Set to
  // units::Timestamp::Zero() if no valid timestamp is available.
  units::Timestamp last_acked_packet_sent_time_;

  // The time at which the most recent packet was acknowledged.
  units::Timestamp last_acked_packet_ack_time_;

  bool is_app_limited_ = false;

  // The packet that will be acknowledged after this one will cause the sampler
  // to exit the app-limited phase.
  units::Timestamp end_of_app_limited_phase_;

  // The number of the round trips that have occurred during the connection.
  BbrRoundTripCount round_trip_count_;

  // The send time of the most recently sent packet.
  units::Timestamp last_send_time_;

  // Acknowledgement of any packet after |current_round_trip_end_| will cause
  // the round trip counter to advance.
  units::Timestamp current_round_trip_end_;

  // The filter that tracks the maximum bandwidth over the multiple recent
  // round-trips.
  MaxBandwidthFilter max_bandwidth_;

  units::DataRate default_bandwidth_;

  // Tracks the maximum number of bytes acked faster than the sending rate.
  MaxAckHeightFilter max_ack_height_;

  // The time this aggregation started and the number of bytes acked during it.
  units::Timestamp aggregation_epoch_start_time_;
  units::DataSize aggregation_epoch_bytes_;

  // The number of bytes acknowledged since the last time bytes in flight
  // dropped below the target window.
  units::DataSize bytes_acked_since_queue_drained_;

  // The muliplier for calculating the max amount of extra CWND to add to
  // compensate for ack aggregation.
  float max_aggregation_bytes_multiplier_;

  // Minimum RTT estimate.  Automatically expires within 10 seconds (and
  // triggers PROBE_RTT mode) if no new value is sampled during that period.
  units::TimeDelta min_rtt_;
  units::TimeDelta last_rtt_;
  // The time at which the current value of |min_rtt_| was assigned.
  units::Timestamp min_rtt_timestamp_;

  // The maximum allowed number of bytes in flight.
  units::DataSize congestion_window_;

  // The initial value of the |congestion_window_|.
  units::DataSize initial_congestion_window_;

  // The largest value the |congestion_window_| can achieve.
  units::DataSize max_congestion_window_;

  // The current pacing rate of the connection.
  units::DataRate pacing_rate_;

  // The gain currently applied to the pacing rate.
  double pacing_gain_;
  // The gain currently applied to the congestion window.
  double congestion_window_gain_;

  // The gain used for the congestion window during PROBE_BW.  Latched from
  // quic_bbr_cwnd_gain flag.
  const float congestion_window_gain_constant_;
  // The coefficient by which mean RTT variance is added to the congestion
  // window.  Latched from quic_bbr_rtt_variation_weight flag.
  const float rtt_variance_weight_;
  // If true, exit startup if 1RTT has passed with no bandwidth increase and
  // the connection is in recovery.
  bool exit_startup_on_loss_;

  // Number of round-trips in PROBE_BW mode, used for determining the current
  // pacing gain cycle.
  int cycle_current_offset_;
  // The time at which the last pacing gain cycle was started.
  units::Timestamp last_cycle_start_;

  // Indicates whether the connection has reached the full bandwidth mode.
  bool is_at_full_bandwidth_;
  // Number of rounds during which there was no significant bandwidth increase.
  BbrRoundTripCount rounds_without_bandwidth_gain_;
  // The bandwidth compared to which the increase is measured.
  units::DataRate bandwidth_at_last_round_;

  // Set to true upon exiting quiescence.
  bool exiting_quiescence_;

  // Time at which PROBE_RTT has to be exited.  Setting it to zero indicates
  // that the time is yet unknown as the number of packets in flight has not
  // reached the required value.
  units::Timestamp exit_probe_rtt_at_;
  // Indicates whether a round-trip has passed since PROBE_RTT became active.
  bool probe_rtt_round_passed_;

  // Indicates whether the most recent bandwidth sample was marked as
  // app-limited.
  bool last_sample_is_app_limited_;

  // Current state of recovery.
  RecoveryState recovery_state_;
  // Receiving acknowledgement of a packet after |end_recovery_at_| will cause
  // BBR to exit the recovery mode.  A value above zero indicates at least one
  // loss has been detected, so it must not be set back to zero.
  units::Timestamp end_recovery_at_;
  // A window used to limit the number of bytes in flight during loss recovery.
  units::DataSize recovery_window_;

  bool app_limited_since_last_probe_rtt_;
  units::TimeDelta min_rtt_since_last_probe_rtt_;
  std::string last_update_state_;

  NetworkAvailability::MessageHandler NetworkAvailabilityHandler;
  NetworkRouteChange::MessageHandler NetworkRouteChangeHandler;
  ProcessInterval::MessageHandler ProcessIntervalHandler;
  RemoteBitrateReport::MessageHandler RemoteBitrateReportHandler;
  RoundTripTimeReport::MessageHandler RoundTripTimeReportHandler;
  SentPacket::MessageHandler SentPacketHandler;
  StreamsConfig::MessageHandler StreamsConfigHandler;
  TargetRateConstraints::MessageHandler TargetRateConstraintsHandler;
  TransportLossReport::MessageHandler TransportLossReportHandler;
  TransportPacketsFeedback::MessageHandler TransportPacketsFeedbackHandler;

  RTC_DISALLOW_COPY_AND_ASSIGN(BbrNetworkController);
};

std::ostream& operator<<(std::ostream& os,
                         const BbrNetworkController::Mode& mode);
std::ostream& operator<<(std::ostream& os,
                         const BbrNetworkController::DebugState& state);

}  // namespace bbr
}  // namespace network
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_BBR_NETWORK_CONTROLLER_H_
