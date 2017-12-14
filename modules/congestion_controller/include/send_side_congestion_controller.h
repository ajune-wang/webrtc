/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/paced_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "network_control/include/network_control.h"
#include "network_control/include/network_types.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/race_checker.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class Clock;
class RateLimiter;
class RtcEventLog;
namespace internal {

struct OutstandingData : network::signal::Message<OutstandingData> {
  network::units::DataSize in_flight_data;
};

struct PacerQueueUpdate : network::signal::Message<PacerQueueUpdate> {
  network::units::TimeDelta expected_queue_time;
};

class PacerController {
 public:
  PacerController(const Clock* clock,
                  PacedSender* pacer,
                  rtc::TaskQueue* task_queue,
                  rtc::CriticalSection* lock);
  ~PacerController();
  network::CongestionWindow::HandlingReceiver::uptr CongestionWindowReceiver;
  network::NetworkAvailability::HandlingReceiver::uptr
      NetworkAvailabilityReceiver;
  OutstandingData::HandlingReceiver::uptr OutstandingDataReceiver;
  network::PacerConfig::HandlingReceiver::uptr PacerConfigReceiver;
  network::ProbeClusterConfig::HandlingReceiver::uptr
      ProbeClusterConfigReceiver;

 private:
  void OnCongestionWindow(network::CongestionWindow);
  void OnPacerConfig(network::PacerConfig);
  void OnProbeClusterConfig(network::ProbeClusterConfig);
  void OnOutstandingData(OutstandingData);
  void OnNetworkAvailability(network::NetworkAvailability);

  void OnCongestionInvalidation();
  void SetPacerState(bool paused);

  const Clock* const clock_;
  PacedSender* const pacer_;

  rtc::Optional<network::PacerConfig> current_pacer_config_;
  rtc::Optional<network::CongestionWindow> congestion_window_;
  size_t num_outstanding_bytes_ = 0;
  bool pacer_paused_ = false;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PacerController);
};

class EncodingRateController;
}  // namespace internal
namespace network {
namespace signal {
extern template struct Message<webrtc::internal::OutstandingData>;
extern template struct Message<webrtc::internal::PacerQueueUpdate>;
}  // namespace signal
}  // namespace network
class SendSideCongestionController : public CallStatsObserver,
                                     public Module,
                                     public TransportFeedbackObserver,
                                     public RtcpBandwidthObserver {
 public:
  // Observer class for bitrate changes announced due to change in bandwidth
  // estimate or due to that the send pacer is full. Fraction loss and rtt is
  // also part of this callback to allow the observer to optimize its settings
  // for different types of network environments. The bitrate does not include
  // packet headers and is measured in bits per second.
  class Observer {
   public:
    virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                  uint8_t fraction_loss,  // 0 - 255.
                                  int64_t rtt_ms,
                                  int64_t probing_interval_ms) = 0;

   protected:
    virtual ~Observer() {}
  };
  SendSideCongestionController(const Clock* clock,
                               Observer* observer,
                               RtcEventLog* event_log,
                               PacedSender* pacer);
  ~SendSideCongestionController() override;

  void RegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);
  void DeRegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);

  // Currently, there can be at most one observer.
  // TODO(nisse): The RegisterNetworkObserver method is needed because we first
  // construct this object (as part of RtpTransportControllerSend), then pass a
  // reference to Call, which then registers itself as the observer. We should
  // try to break this circular chain of references, and make the observer a
  // construction time constant.
  void RegisterNetworkObserver(Observer* observer);
  void DeRegisterNetworkObserver(Observer* observer);

  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);
  // Resets the BWE state. Note the first argument is the bitrate_bps.
  virtual void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                                     int bitrate_bps,
                                     int min_bitrate_bps,
                                     int max_bitrate_bps);
  virtual void SignalNetworkState(NetworkState state);
  virtual void SetTransportOverhead(size_t transport_overhead_bytes_per_packet);

  virtual RtcpBandwidthObserver* GetBandwidthObserver();

  virtual bool AvailableBandwidth(uint32_t* bandwidth) const;
  virtual int64_t GetPacerQueuingDelayMs() const;
  virtual int64_t GetFirstPacketTimeMs() const;

  virtual TransportFeedbackObserver* GetTransportFeedbackObserver();

  RateLimiter* GetRetransmissionRateLimiter();
  void EnablePeriodicAlrProbing(bool enable);

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet);

  // Implements RtcpBandwidthObserver
  void OnReceivedEstimatedBitrate(uint32_t bitrate) override;
  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override;

  // Ignored
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Implements TransportFeedbackObserver.
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
  std::vector<PacketFeedback> GetTransportFeedbackVector() const override;

  // Sets the minimum send bitrate and maximum padding bitrate requested by send
  // streams.
  // |min_send_bitrate_bps| might be higher that the estimated available network
  // bitrate and if so, the pacer will send with |min_send_bitrate_bps|.
  // |max_padding_bitrate_bps| might be higher than the estimate available
  // network bitrate and if so, the pacer will send padding packets to reach
  // the min of the estimated available bitrate and |max_padding_bitrate_bps|.
  void SetSendBitrateLimits(int64_t min_send_bitrate_bps,
                            int64_t max_padding_bitrate_bps);
  void SetPacingFactor(float pacing_factor);
  float GetPacingFactor() const;

 protected:
  // Waits long enough that any outstanding tasks should be finished. Used by
  // unit tests.
  void Sync();

 private:
  void Wait();

  void MaybeUpdateOutstandingData();
  void OnReceivedRtcpReceiverReportBlocks(const ReportBlockList& report_blocks,
                                          int64_t now_ms);

  const Clock* const clock_;
  std::unique_ptr<rtc::TaskQueue> task_queue_;
  RtcEventLog* const event_log_;
  PacedSender* const pacer_;

  rtc::CriticalSection controller_lock_;
  TransportFeedbackAdapter transport_feedback_adapter_;

  int64_t last_process_update_ms_ = 0;

  std::map<uint32_t, RTCPReportBlock> last_report_blocks_;
  network::units::Timestamp last_report_block_time_;

  rtc::CriticalSection streams_config_lock_;
  network::StreamsConfig streams_config_ RTC_GUARDED_BY(streams_config_lock_);

  rtc::RaceChecker worker_race_;

  bool using_task_queue_;

  // Receivers are declared in the end to make sure they could not access
  // desctructed internals.
  network::TargetTransferRate::CacheReceiver TargetTransferRateCache;
  network::CongestionWindow::CacheReceiver CongestionWindowCache;
  network::NetworkAvailability::CacheReceiver NetworkAvailabilityCache;

  std::unique_ptr<internal::EncodingRateController> encoding_rate_controller_;
  std::unique_ptr<internal::PacerController> pacer_controller_;

  // The network controller should be created after the other controlles so they
  // can handle any messages from the network controller.
  std::unique_ptr<network::NetworkControllerInterface> controller_;

  // TODO(srte): Theese should be moved closer to where the messages are
  // generated and only connected/disconnected in the wrapper class.

  // Junctions are created last so they could not be used after the controller
  // has been destructed.
  network::SentPacket::Junction SentPacketJunction;
  network::TransportPacketsFeedback::Junction TransportPacketsFeedbackJunction;
  network::TransportLossReport::Junction TransportLossReportJunction;
  network::RoundTripTimeReport::Junction RoundTripTimeReportJunction;
  network::RemoteBitrateReport::Junction RemoteBitrateReportJunction;
  network::TargetRateConstraints::Junction TransferRateConstraintsJunction;
  network::StreamsConfig::Junction StreamsConfigJunction;
  network::NetworkRouteChange::Junction NetworkRouteChangeJunction;
  network::ProcessInterval::Junction ProcessIntervalJunction;

  // Junctions used by other controllers
  internal::PacerQueueUpdate::Junction PacerQueueUpdateJunction;
  internal::OutstandingData::Junction OutstandingDataJunction;
  network::NetworkAvailability::Junction NetworkAvailabilityJunction;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendSideCongestionController);
};
namespace internal {
// This class converts network messages to legacy API
class EncodingRateController {
 public:
  EncodingRateController(const Clock* clock,
                         rtc::TaskQueue* task_queue,
                         rtc::CriticalSection* lock);
  ~EncodingRateController();
  void RegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);
  void DeRegisterNetworkObserver(
      SendSideCongestionController::Observer* observer);

  RateLimiter* GetRetransmissionRateLimiter();

  internal::PacerQueueUpdate::HandlingReceiver::uptr PacerQueueUpdateReceiver;
  network::TargetTransferRate::HandlingReceiver::uptr
      TargetTransferRateReceiver;
  network::NetworkAvailability::HandlingReceiver::uptr
      NetworkAvailabilityReceiver;

 private:
  void OnNetworkAvailability(network::NetworkAvailability);
  void OnTargetTransferRate(network::TargetTransferRate);
  void OnPacerQueueUpdate(internal::PacerQueueUpdate);

  void OnNetworkInvalidation();

  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms);
  bool IsSendQueueFull() const;
  bool HasNetworkParametersToReportChanged(int64_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  rtc::CriticalSection observer_lock_;
  SendSideCongestionController::Observer* observer_
      RTC_GUARDED_BY(observer_lock_) = nullptr;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  rtc::Optional<network::TargetTransferRate> current_target_rate_;

  bool network_available_ = true;
  int64_t last_reported_target_bitrate_bps_;
  uint8_t last_reported_fraction_loss_;
  int64_t last_reported_rtt_ms_;
  bool pacer_pushback_experiment_ = false;
  int64_t pacer_expected_queue_ms_ = 0;
  float encoding_rate_ = 1.0;
};
}  // namespace internal
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
