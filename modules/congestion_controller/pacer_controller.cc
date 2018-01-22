/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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

class PacerController::Impl {
 public:
  Impl(const Clock* clock, PacedSender* pacer)
      : clock_(clock), pacer_(pacer), pacer_configured_(false) {}
  ~Impl() = default;

  void OnCongestionWindow(CongestionWindow);
  void OnNetworkAvailability(NetworkAvailability);
  void OnNetworkRouteChange(NetworkRouteChange);
  void OnOutstandingData(OutstandingData);
  void OnPacerConfig(PacerConfig);
  void OnProbeClusterConfig(ProbeClusterConfig);

  void SetPacerState(bool paused);

  const Clock* const clock_;
  PacedSender* const pacer_;

  rtc::Optional<PacerConfig> current_pacer_config_;
  bool pacer_paused_ = false;
  bool network_available_ = true;
  std::atomic<bool> pacer_configured_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Impl);
};

PacerController::PacerController(const Clock* clock, PacedSender* pacer)
    : impl_(rtc::MakeUnique<Impl>(clock, pacer)) {
  CongestionWindowReceiver.Bind(impl_.get(), &Impl::OnCongestionWindow);
  NetworkAvailabilityReceiver.Bind(impl_.get(), &Impl::OnNetworkAvailability);
  NetworkRouteChangeReceiver.Bind(impl_.get(), &Impl::OnNetworkRouteChange);
  PacerConfigReceiver.Bind(impl_.get(), &Impl::OnPacerConfig);
  ProbeClusterConfigReceiver.Bind(impl_.get(), &Impl::OnProbeClusterConfig);
  OutstandingDataReceiver.Bind(impl_.get(), &Impl::OnOutstandingData);
}

PacerController::~PacerController() {}

bool PacerController::GetPacerConfigured() {
  return impl_->pacer_configured_.load();
}

void PacerController::Impl::OnCongestionWindow(
    CongestionWindow congestion_window) {
  if (congestion_window.enabled)
    pacer_->SetCongestionWindow(congestion_window.data_window.bytes());
}

void PacerController::Impl::OnNetworkAvailability(NetworkAvailability msg) {
  network_available_ = msg.network_available;
  pacer_->SetOutstandingData(0);
  SetPacerState(!msg.network_available);
}

void PacerController::Impl::OnNetworkRouteChange(NetworkRouteChange) {
  pacer_->SetOutstandingData(0);
}

void PacerController::Impl::OnPacerConfig(PacerConfig msg) {
  DataRate pacing_rate = msg.data_window / msg.time_window;
  DataRate padding_rate = msg.pad_window / msg.time_window;
  pacer_->SetPacingRates(pacing_rate.bps(), padding_rate.bps());
  pacer_configured_.store(true);
}

void PacerController::Impl::OnProbeClusterConfig(ProbeClusterConfig config) {
  int64_t bitrate_bps = config.target_data_rate.bps();
  pacer_->CreateProbeCluster(bitrate_bps);
}

void PacerController::Impl::OnOutstandingData(OutstandingData msg) {
  // num_outstanding_bytes_ = msg.in_flight_data.bytes();
  pacer_->SetOutstandingData(msg.in_flight_data.bytes());
}

void PacerController::Impl::SetPacerState(bool paused) {
  if (paused && !pacer_paused_)
    pacer_->Pause();
  else if (!paused && pacer_paused_)
    pacer_->Resume();
  pacer_paused_ = paused;
}

}  // namespace webrtc
