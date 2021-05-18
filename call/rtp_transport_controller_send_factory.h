/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_FACTORY_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_FACTORY_H_

#include <memory>
#include <utility>

#include "call/rtp_transport_controller_send.h"
#include "call/rtp_transport_controller_send_factory_interface.h"

namespace webrtc {
class RtpTransportControllerSendFactory
    : public RtpTransportControllerSendFactoryInterface {
 public:
  std::unique_ptr<RtpTransportControllerSendInterface> create(
      Clock* clock,
      RtcEventLog* event_log,
      NetworkStatePredictorFactoryInterface* predictor_factory,
      NetworkControllerFactoryInterface* controller_factory,
      const BitrateConstraints& bitrate_config,
      std::unique_ptr<ProcessThread> process_thread,
      TaskQueueFactory* task_queue_factory,
      const WebRtcKeyValueConfig* trials) override {
    return std::make_unique<RtpTransportControllerSend>(
        clock, event_log, predictor_factory, controller_factory, bitrate_config,
        std::move(process_thread), task_queue_factory, trials);
  }

  virtual ~RtpTransportControllerSendFactory() {}
};
}  // namespace webrtc
#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_FACTORY_H_
