/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_

#include <array>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class ApmDataDumper;

class StationarityEstimator {
 public:
  enum class SignalType { kNonStationary, kStationary };

  StationarityEstimator();
  ~StationarityEstimator();

  // Classify the signal and update the signal statistics.
  bool Update(rtc::ArrayView<const float> spectrum);

  // Classify the signal without updating the signal statistics.
  bool Analyze(rtc::ArrayView<const float> spectrum) const;

  // Returns true if that band for the current frame is stationary.
  bool IsBandStationary(size_t k) const;

  // Returns the power of the stationary noise spectrum at a band.
  float GetStationarityPowerBand(size_t k) const { return noise_.PowerBand(k); }

  // Returns the power of the current smooth spectrum at a band.
  // JVP. probably move to the render/input class
  float GetPowerBandSmthSpectrum(size_t k) const { return smooth_spectrum_[k]; }

  // Returns the power of the stationary noise spectrum.
  float GetStationaryPower() const { return noise_.Power(); }

  // Returns whether the current spectrum is or not stationary.
  bool IsFrameStationary();

 private:
  // Smooth the input spectrum for a better stationary estimation.
  void ComputeSmoothSpectrum(rtc::ArrayView<const float> signal_spectrum);

  // Update the stationary status per band for the current frame
  void UpdateStationaryStatus();

  class NoiseSpectrum {
   public:
    NoiseSpectrum();
    ~NoiseSpectrum();
    void Update(rtc::ArrayView<const float> spectrum);
    float GetAlpha() const;
    rtc::ArrayView<const float> Spectrum() const {
      return rtc::ArrayView<const float>(noise_spectrum_);
    }

    float Power() const { return power_; }
    float PowerBand(size_t band) const;

   private:
    static constexpr float alpha = 0.004f;  // 1 second time constant
    static constexpr float alpha_ini = 0.1f;
    static constexpr float nBlocksInitialPhase = kNumBlocksPerSecond * 10;
    static constexpr float tilt_alpha =
        (alpha_ini - alpha) / nBlocksInitialPhase;
    std::array<float, kFftLengthBy2Plus1> noise_spectrum_;
    float power_;
    size_t block_counter_;
  };

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  NoiseSpectrum noise_;
  std::array<bool, kFftLengthBy2Plus1> stationary_;
  size_t stationarity_counter = 3;
  bool stationary_frame_ = false;
  std::array<float, kFftLengthBy2Plus1> smooth_spectrum_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
