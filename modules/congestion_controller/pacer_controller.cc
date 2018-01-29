/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/pacer_controller.h"

#include "network_control/include/network_units.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

PacerController::PacerController(PacedSender* pacer) : pacer_(pacer) {}

PacerController::~PacerController() = default;

void PacerController::OnCongestionWindow(CongestionWindow congestion_window) {
  if (congestion_window.enabled)
    pacer_->SetCongestionWindow(congestion_window.data_window.bytes());
}

void PacerController::OnNetworkAvailability(NetworkAvailability msg) {
  network_available_ = msg.network_available;
  pacer_->SetOutstandingData(0);
  SetPacerState(!msg.network_available);
}

void PacerController::OnNetworkRouteChange(NetworkRouteChange) {
  pacer_->SetOutstandingData(0);
}

void PacerController::OnPacerConfig(PacerConfig msg) {
  DataRate pacing_rate = msg.data_window / msg.time_window;
  DataRate padding_rate = msg.pad_window / msg.time_window;
  pacer_->SetPacingRates(pacing_rate.bps(), padding_rate.bps());
}

void PacerController::OnProbeClusterConfig(ProbeClusterConfig config) {
  int64_t bitrate_bps = config.target_data_rate.bps();
  pacer_->CreateProbeCluster(bitrate_bps);
}

void PacerController::OnOutstandingData(OutstandingData msg) {
  pacer_->SetOutstandingData(msg.in_flight_data.bytes());
}

void PacerController::SetPacerState(bool paused) {
  if (paused && !pacer_paused_)
    pacer_->Pause();
  else if (!paused && pacer_paused_)
    pacer_->Resume();
  pacer_paused_ = paused;
}

}  // namespace webrtc
