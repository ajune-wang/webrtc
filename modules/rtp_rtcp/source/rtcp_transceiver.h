/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_

#include <memory>
#include <string>

#include "rtc_base/constructormagic.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/weak_ptr.h"

namespace rtc {
class TaskQueue;
}  // namespace rtc

namespace webrtc {
class ReceiveStatisticsProvider;
class RtcpTransceiverImpl;
class Transport;
//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is thread-safe wrapper of RtcpTransceiverImpl
class RtcpTransceiver {
 public:
  struct Configuration {
    Configuration();
    ~Configuration();
    // Logs the error and returns false if configuration miss key objects or
    // is inconsistant. May log warnings.
    bool Valid() const;

    // Used to prepend all log messages. Can be empty.
    std::string debug_id;

    // Ssrc to use for transport-wide feedbacks.
    uint32_t feedback_ssrc = 1;

    // cname of the local particiapnt.
    std::string cname;

    // Maximum packet size outgoing transport accepts.
    size_t max_packet_size = 1200;
    // Transport to send rtcp packets to. Should be set.
    Transport* outgoing_transport = nullptr;

    // Min/max Period to send receiver reports and attached messages.
    int min_periodic_report_ms = 1000;
    // class to use to generate report blocks in receiver reports.
    ReceiveStatisticsProvider* receive_statistics = nullptr;

    //
    // Flags for features, experiments, etc. Use at your own risk.
    //
    // Set to false to manually decide when to send Sender/Receiver Report.
    // When set, RtcpTransceiver should be used from the same rtc::TaskQueue.
    bool schedule_periodic_reports = true;
  };

  RtcpTransceiver(rtc::TaskQueue* task_queue, const Configuration& config);
  ~RtcpTransceiver();

  // Process incoming rtcp packet.
  void ReceivePacket(rtc::CopyOnWriteBuffer packet);

  // Sends sender/receiver report asap.
  void ForceSendReport();

 private:
  rtc::TaskQueue* const task_queue_;
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver_;
  std::unique_ptr<rtc::WeakPtrFactory<RtcpTransceiverImpl>> ptr_factory_;
  // TaskQueue, and thus tasks posted to it, may outlive this.
  // Thus when Posting task class always pass copy of the weak_ptr to access
  // the RtcpTransceiver and never guarantee it still will be alive when task
  // runs.
  rtc::WeakPtr<RtcpTransceiverImpl> ptr_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiver);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_
