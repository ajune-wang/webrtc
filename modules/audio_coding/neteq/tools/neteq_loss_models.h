/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_LOSS_MODELS_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_LOSS_MODELS_H_

#include <memory>
#include <set>

namespace webrtc {
namespace test {

enum LossModes {
  kNoLoss,
  kUniformLoss,
  kGilbertElliotLoss,
  kFixedLoss,
  kLastLossMode
};

class LossModel {
 public:
  virtual ~LossModel() {}
  virtual bool Lost(int now_ms) = 0;
};

class NoLoss : public LossModel {
 public:
  bool Lost(int now_ms) override;
};

class UniformLoss : public LossModel {
 public:
  explicit UniformLoss(double loss_rate);
  bool Lost(int now_ms) override;
  void set_loss_rate(double loss_rate) { loss_rate_ = loss_rate; }

 private:
  double loss_rate_;
};

class GilbertElliotLoss : public LossModel {
 public:
  GilbertElliotLoss(double prob_trans_11, double prob_trans_01);
  ~GilbertElliotLoss() override;
  bool Lost(int now_ms) override;

 private:
  // Prob. of losing current packet, when previous packet is lost.
  double prob_trans_11_;
  // Prob. of losing current packet, when previous packet is not lost.
  double prob_trans_01_;
  bool lost_last_;
  std::unique_ptr<UniformLoss> uniform_loss_model_;
};

struct FixedLossEvent {
  int start_ms;
  int duration_ms;
  FixedLossEvent(int start_ms, int duration_ms)
      : start_ms(start_ms), duration_ms(duration_ms) {}
};

struct FixedLossEventCmp {
  bool operator()(const FixedLossEvent& l_event,
                  const FixedLossEvent& r_event) const {
    return l_event.start_ms < r_event.start_ms;
  }
};

class FixedLossModel : public LossModel {
 public:
  explicit FixedLossModel(
      std::set<FixedLossEvent, FixedLossEventCmp> loss_events);
  ~FixedLossModel() override;
  bool Lost(int now_ms) override;

 private:
  std::set<FixedLossEvent, FixedLossEventCmp> loss_events_;
  std::set<FixedLossEvent, FixedLossEventCmp>::iterator loss_events_it_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_LOSS_MODELS_H_
