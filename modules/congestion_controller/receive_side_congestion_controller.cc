/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/receive_side_congestion_controller.h"

#include "modules/pacing/packet_router.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

namespace {
static const uint32_t kTimeOffsetSwitchThreshold = 30;
}  // namespace

ReceiveSideCongestionController::WrappingBitrateEstimator::
    WrappingBitrateEstimator(RemoteBitrateObserver* observer,
                             Clock* clock,
                             TaskQueueBase* task_queue)
    : observer_(observer),
      clock_(clock),
      task_queue_(task_queue),
      rbe_(new RemoteBitrateEstimatorSingleStream(observer_, clock_)),
      using_absolute_send_time_(false),
      packets_since_absolute_send_time_(0),
      min_bitrate_bps_(congestion_controller::GetMinBitrateBps()) {
  StartRemoteBitrateEstimatorOnTaskQueue();
}

ReceiveSideCongestionController::WrappingBitrateEstimator::
    ~WrappingBitrateEstimator() {
  if (!using_absolute_send_time_) {
    StopAndDeleteRemoteBitrateEstimatorOnTaskQueue();
  }
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::
    StopAndDeleteRemoteBitrateEstimatorOnTaskQueue() {
  if (!task_queue_)
    return;

  task_queue_->PostTask(ToQueuedTask([rbe_updater_ = std::move(rbe_updater_),
                                      rbe_ = std::move(rbe_)]() mutable {
    rbe_updater_.Stop();
    rbe_ = nullptr;
  }));
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::
    StartRemoteBitrateEstimatorOnTaskQueue() {
  RTC_DCHECK(!using_absolute_send_time_);
  if (!task_queue_)
    return;
  RemoteBitrateEstimatorSingleStream* estimator =
      static_cast<RemoteBitrateEstimatorSingleStream*>(rbe_.get());
  rbe_updater_ = RepeatingTaskHandle::Start(
      task_queue_, [estimator] { return estimator->PeriodicProcess(); });
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::IncomingPacket(
    int64_t arrival_time_ms,
    size_t payload_size,
    const RTPHeader& header) {
  rtc::CritScope cs(&crit_sect_);
  PickEstimatorFromHeader(header);
  rbe_->IncomingPacket(arrival_time_ms, payload_size, header);
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::Process() {
  rtc::CritScope cs(&crit_sect_);
  rbe_->Process();
}

int64_t ReceiveSideCongestionController::WrappingBitrateEstimator::
    TimeUntilNextProcess() {
  rtc::CritScope cs(&crit_sect_);
  return rbe_->TimeUntilNextProcess();
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::OnRttUpdate(
    int64_t avg_rtt_ms,
    int64_t max_rtt_ms) {
  rtc::CritScope cs(&crit_sect_);
  rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::RemoveStream(
    unsigned int ssrc) {
  rtc::CritScope cs(&crit_sect_);
  rbe_->RemoveStream(ssrc);
}

bool ReceiveSideCongestionController::WrappingBitrateEstimator::LatestEstimate(
    std::vector<unsigned int>* ssrcs,
    unsigned int* bitrate_bps) const {
  rtc::CritScope cs(&crit_sect_);
  return rbe_->LatestEstimate(ssrcs, bitrate_bps);
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::SetMinBitrate(
    int min_bitrate_bps) {
  rtc::CritScope cs(&crit_sect_);
  rbe_->SetMinBitrate(min_bitrate_bps);
  min_bitrate_bps_ = min_bitrate_bps;
}

void ReceiveSideCongestionController::WrappingBitrateEstimator::
    PickEstimatorFromHeader(const RTPHeader& header) {
  if (header.extension.hasAbsoluteSendTime) {
    // If we see AST in header, switch RBE strategy immediately.
    if (!using_absolute_send_time_) {
      RTC_LOG(LS_INFO)
          << "WrappingBitrateEstimator: Switching to absolute send time RBE.";
      using_absolute_send_time_ = true;
      PickEstimator();
    }
    packets_since_absolute_send_time_ = 0;
  } else {
    // When we don't see AST, wait for a few packets before going back to TOF.
    if (using_absolute_send_time_) {
      ++packets_since_absolute_send_time_;
      if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
        RTC_LOG(LS_INFO)
            << "WrappingBitrateEstimator: Switching to transmission "
               "time offset RBE.";
        using_absolute_send_time_ = false;
        PickEstimator();
      }
    }
  }
}

// Instantiate RBE for Time Offset or Absolute Send Time extensions.
void ReceiveSideCongestionController::WrappingBitrateEstimator::
    PickEstimator() {
  if (using_absolute_send_time_) {
    StopAndDeleteRemoteBitrateEstimatorOnTaskQueue();
    rbe_ =
        std::make_unique<RemoteBitrateEstimatorAbsSendTime>(observer_, clock_);
  } else {
    rbe_ =
        std::make_unique<RemoteBitrateEstimatorSingleStream>(observer_, clock_);
    StartRemoteBitrateEstimatorOnTaskQueue();
  }
  rbe_->SetMinBitrate(min_bitrate_bps_);
}

ReceiveSideCongestionController::ReceiveSideCongestionController(
    Clock* clock,
    TaskQueueBase* task_queue,
    PacketRouter* packet_router,
    NetworkStateEstimator* network_state_estimator)
    : task_queue_(task_queue),
      remote_bitrate_estimator_(packet_router, clock, task_queue),
      remote_estimator_proxy_(
          std::make_unique<RemoteEstimatorProxy>(clock,
                                                 task_queue,
                                                 packet_router,
                                                 &field_trial_config_,
                                                 network_state_estimator)) {}

ReceiveSideCongestionController::~ReceiveSideCongestionController() {
  if (task_queue_) {
    // Delete the remote_estimator_proxy_ on the task queue to avoid race
    // with it's periodic task.
    RTC_LOG(LS_INFO) << "destroying";
    task_queue_->PostTask(
        ToQueuedTask([remote_estimator_proxy_ =
                          std::move(remote_estimator_proxy_)]() mutable {
          remote_estimator_proxy_->Stop();
          remote_estimator_proxy_ = nullptr;
        }));
  }
}

void ReceiveSideCongestionController::OnReceivedPacket(
    int64_t arrival_time_ms,
    size_t payload_size,
    const RTPHeader& header) {
  remote_estimator_proxy_->IncomingPacket(arrival_time_ms, payload_size,
                                          header);
  if (!header.extension.hasTransportSequenceNumber) {
    // Receive-side BWE.
    remote_bitrate_estimator_.IncomingPacket(arrival_time_ms, payload_size,
                                             header);
  }
}

void ReceiveSideCongestionController::SetSendPeriodicFeedback(
    bool send_periodic_feedback) {
  remote_estimator_proxy_->SetSendPeriodicFeedback(send_periodic_feedback);
}

RemoteBitrateEstimator*
ReceiveSideCongestionController::GetRemoteBitrateEstimator(bool send_side_bwe) {
  if (send_side_bwe) {
    return remote_estimator_proxy_.get();
  } else {
    return &remote_bitrate_estimator_;
  }
}

const RemoteBitrateEstimator*
ReceiveSideCongestionController::GetRemoteBitrateEstimator(
    bool send_side_bwe) const {
  if (send_side_bwe) {
    return remote_estimator_proxy_.get();
  } else {
    return &remote_bitrate_estimator_;
  }
}

void ReceiveSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                                  int64_t max_rtt_ms) {
  remote_bitrate_estimator_.OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

void ReceiveSideCongestionController::OnBitrateChanged(int bitrate_bps) {
  remote_estimator_proxy_->OnBitrateChanged(bitrate_bps);
}

int64_t ReceiveSideCongestionController::TimeUntilNextProcess() {
  return remote_bitrate_estimator_.TimeUntilNextProcess();
}

void ReceiveSideCongestionController::Process() {
  remote_bitrate_estimator_.Process();
}

}  // namespace webrtc
