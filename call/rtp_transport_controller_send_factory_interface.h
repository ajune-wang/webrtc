/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_FACTORY_INTERFACE_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_FACTORY_INTERFACE_H_

#include <memory>

#include "api/network_state_predictor.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/network_control.h"
#include "api/transport/webrtc_key_value_config.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
// A factory used for dependency injection on the send side of the transport
// controller.
class RtpTransportControllerSendFactoryInterface {
 public:
  virtual std::unique_ptr<RtpTransportControllerSendInterface> create(
      Clock* clock,
      RtcEventLog* event_log,
      NetworkStatePredictorFactoryInterface* predictor_factory,
      NetworkControllerFactoryInterface* controller_factory,
      const BitrateConstraints& bitrate_config,
      std::unique_ptr<ProcessThread> process_thread,
      TaskQueueFactory* task_queue_factory,
      const WebRtcKeyValueConfig* trials) = 0;
  virtual ~RtpTransportControllerSendFactoryInterface() {}
};
}  // namespace webrtc
#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_FACTORY_INTERFACE_H_
