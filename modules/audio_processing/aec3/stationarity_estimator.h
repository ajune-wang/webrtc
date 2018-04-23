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
#include <memory>
#include <vector>
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class ApmDataDumper;

class StationarityEstimator {
 public:
  enum class SignalType { kNonStationary, kStationary };

  StationarityEstimator();
  ~StationarityEstimator();

  void Reset();

  // Update the stationary estimator
  void Update(rtc::ArrayView<const float> spectrum,
              size_t block_number,
              bool first_block);  // JVP that first_block is just for debugging

  // Update the flag indicating whether the ahead blocks are all stationary.
  void UpdateFlagsStationaryAhead(int current_block_number, int num_lookahead);

  // Returns true if ahead the current point just stationary signals
  // at a band are expected.
  bool IsBandStationaryAhead(size_t k) const {
    return (all_ahead_stationary_[k] && (hangover_long_window_[k] == 0));
  }

 private:
  // Returns the power of the stationary noise spectrum at a band.
  float GetStationarityPowerBand(size_t k) const { return noise_.PowerBand(k); }

  // Update the stationary status per band for the current frame
  void UpdateStationaryStatus(int block_number,
                              rtc::ArrayView<const float> spectrum);

  bool IsBandStationaryLongWindow(const std::vector<size_t>& idx_lookahead,
                                  const std::vector<size_t>& idx_lookback,
                                  int k) const;

  bool AreAllBandsStationary();

  void UpdateHangoverLongWindow();

  void GetSlotsAheadBack(std::vector<size_t>* idx_lookahead,
                         std::vector<size_t>* idx_lookback,
                         size_t current_block_number,
                         int num_ahead);

  void SmoothStationaryPerFreq();

  class NoiseSpectrum {
   public:
    NoiseSpectrum();
    ~NoiseSpectrum();
    void Reset();
    void Update(rtc::ArrayView<const float> spectrum);
    float GetAlpha() const;
    rtc::ArrayView<const float> Spectrum() const {
      return rtc::ArrayView<const float>(noise_spectrum_);
    }

    float Power() const { return power_; }
    float PowerBand(size_t band) const;

   private:
    float UpdateBandBySmoothing(float power_band,
                                float power_band_noise,
                                float alpha) const;
    static constexpr float kAlpha = 0.004f;
    static constexpr float kAlphaInit = 0.04f;
    static constexpr float kNBlocksInitialPhase = kNumBlocksPerSecond * 2.;
    static constexpr float kTiltAlpha =
        (kAlphaInit - kAlpha) / kNBlocksInitialPhase;
    static constexpr float kNBlocksAverageInitPhase = 20;
    std::array<float, kFftLengthBy2Plus1> noise_spectrum_;
    float power_;
    size_t block_counter_;
  };

  class CircularBuffer {
   public:
    CircularBuffer();
    static constexpr int kCircularBufferSize = 32;
    bool IsBlockNumberAlreadyUpdated(int block_number) const;
    int GetSlotNumber(int block_number) const {
      return block_number & (kCircularBufferSize - 1);
    }
    int SetBlockNumberInSlot(int block_number);
    void SetElementProperties(float band_power, int slot, int band) {
      slots_[slot].SetElementProperties(band_power, band);
    }

    float GetRatioFrameNoiseSmth(int slot, int band) const {
      return slots_[slot].GetRatioFrameNoiseSmth(band);
    }
    float GetPowerBand(int slot, int band) const {
      return slots_[slot].GetPowerBand(band);
    }

   private:
    class Element {
     public:
      Element();
      int GetBlockNumber() const { return block_number_; }
      void SetBlockNumber(int block_number) { block_number_ = block_number; }
      void SetElementProperties(float band_power, int band) {
        RTC_DCHECK_LT(band, ratio_noise_frame_smooth_.size());
        power_spectrum_[band] = band_power;
      }
      float GetRatioFrameNoiseSmth(int band) const {
        RTC_DCHECK_LT(band, ratio_noise_frame_smooth_.size());
        return ratio_noise_frame_smooth_[band];
      }
      float GetPowerBand(int band) const { return power_spectrum_[band]; }

     private:
      int block_number_;
      std::array<float, kFftLengthBy2Plus1> ratio_noise_frame_smooth_;
      std::array<float, kFftLengthBy2Plus1> power_spectrum_;
    };
    std::array<Element, kCircularBufferSize> slots_;
  };

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  NoiseSpectrum noise_;
  std::array<int, kFftLengthBy2Plus1> hangover_long_window_;
  std::array<bool, kFftLengthBy2Plus1> all_ahead_stationary_;
  CircularBuffer buffer_;
  static constexpr float kMinThr = 10.f;
  static constexpr size_t kLongWindowSize = 12;

 public:
  static constexpr size_t kMaxNumLookahead =
      CircularBuffer::kCircularBufferSize - 2;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
