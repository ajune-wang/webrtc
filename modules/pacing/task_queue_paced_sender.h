/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_TASK_QUEUE_PACED_SENDER_H_
#define MODULES_PACING_TASK_QUEUE_PACED_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/include/module.h"
#include "modules/pacing/pacing_controller.h"
#include "modules/pacing/packet_router.h"
#include "modules/pacing/rtp_packet_pacer.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class Clock;
class RtcEventLog;

class TaskQueuePacedSender : public RtpPacketPacer,
                             public RtpPacketSender,
                             private PacingController::PacketSender {
 public:
  struct Stats {
    Stats()
        : oldest_packet_wait_time(TimeDelta::Zero()),
          queue_size(DataSize::Zero()),
          expected_queue_time(TimeDelta::Zero()) {}
    TimeDelta oldest_packet_wait_time;
    DataSize queue_size;
    TimeDelta expected_queue_time;
    absl::optional<Timestamp> first_sent_packet_time;
  };

  TaskQueuePacedSender(Clock* clock,
                       PacketRouter* packet_router,
                       RtcEventLog* event_log,
                       const WebRtcKeyValueConfig* field_trials,
                       TaskQueueFactory* task_queue_factory);

  ~TaskQueuePacedSender() override;

  // Methods implementing RtpPacketSender.

  // it's time to send.
  void EnqueuePackets(
      std::vector<std::unique_ptr<RtpPacketToSend>> packets) override;

  // Methods implementing RtpPacketPacer:

  void CreateProbeCluster(DataRate bitrate, int cluster_id) override;

  // Temporarily pause all sending.
  void Pause() override;

  // Resume sending packets.
  void Resume() override;

  void SetCongestionWindow(DataSize congestion_window_size) override;
  void UpdateOutstandingData(DataSize outstanding_data) override;

  // Sets the pacing rates. Must be called once before packets can be sent.
  void SetPacingRates(DataRate pacing_rate, DataRate padding_rate) override;

  // Currently audio traffic is not accounted by pacer and passed through.
  // With the introduction of audio BWE audio traffic will be accounted for
  // the pacer budget calculation. The audio traffic still will be injected
  // at high priority.
  void SetAccountForAudioPackets(bool account_for_audio) override;

  // Returns the time since the oldest queued packet was enqueued.
  TimeDelta OldestPacketWaitTime() const override;

  DataSize QueueSizeData() const override;

  // Returns the time when the first packet was sent;
  absl::optional<Timestamp> FirstSentPacketTime() const override;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  TimeDelta ExpectedQueueTime() const override;

  void SetQueueTimeLimit(TimeDelta limit) override;

 private:
  void MaybeProcessPackets(Timestamp scheduled_process_time);

  // Methods implementing PacedSenderController:PacketSender.

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& cluster_info) override
      RTC_RUN_ON(task_queue_);

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize size) override RTC_RUN_ON(task_queue_);

  Stats GetStats() const;

  void Shutdown();
  bool IsShutdown() const;

  Clock* const clock_;
  PacketRouter* const packet_router_ RTC_GUARDED_BY(task_queue_);
  PacingController pacing_controller_ RTC_GUARDED_BY(task_queue_);

  Timestamp next_process_time_;
  Timestamp last_stats_update_;

  rtc::CriticalSection crit_;
  bool shutdown_ RTC_GUARDED_BY(crit_);
  Stats current_stats_ RTC_GUARDED_BY(crit_);

  rtc::TaskQueue task_queue_;
};
}  // namespace webrtc
#endif  // MODULES_PACING_TASK_QUEUE_PACED_SENDER_H_
