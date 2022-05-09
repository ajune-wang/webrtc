/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_loss_models.h"

#include <cmath>

namespace webrtc {
namespace test {

bool NoLoss::Lost(int now_ms) {
  return false;
}

UniformLoss::UniformLoss(double loss_rate) : loss_rate_(loss_rate) {}

bool UniformLoss::Lost(int now_ms) {
  int drop_this = rand();
  return (drop_this < loss_rate_ * RAND_MAX);
}

GilbertElliotLoss::GilbertElliotLoss(double prob_trans_11, double prob_trans_01)
    : prob_trans_11_(prob_trans_11),
      prob_trans_01_(prob_trans_01),
      lost_last_(false),
      uniform_loss_model_(new UniformLoss(0)) {}

GilbertElliotLoss::~GilbertElliotLoss() {}

bool GilbertElliotLoss::Lost(int now_ms) {
  // Simulate bursty channel (Gilbert model).
  // (1st order) Markov chain model with memory of the previous/last
  // packet state (lost or received).
  if (lost_last_) {
    // Previous packet was not received.
    uniform_loss_model_->set_loss_rate(prob_trans_11_);
    return lost_last_ = uniform_loss_model_->Lost(now_ms);
  } else {
    uniform_loss_model_->set_loss_rate(prob_trans_01_);
    return lost_last_ = uniform_loss_model_->Lost(now_ms);
  }
}

FixedLossModel::FixedLossModel(
    std::set<FixedLossEvent, FixedLossEventCmp> loss_events)
    : loss_events_(loss_events) {
  loss_events_it_ = loss_events_.begin();
}

FixedLossModel::~FixedLossModel() {}

bool FixedLossModel::Lost(int now_ms) {
  if (loss_events_it_ != loss_events_.end() &&
      now_ms > loss_events_it_->start_ms) {
    if (now_ms <= loss_events_it_->start_ms + loss_events_it_->duration_ms) {
      return true;
    } else {
      ++loss_events_it_;
      return false;
    }
  }
  return false;
}

}  // namespace test
}  // namespace webrtc
