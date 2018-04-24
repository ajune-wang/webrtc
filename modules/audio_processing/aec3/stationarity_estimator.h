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
  StationarityEstimator();
  ~StationarityEstimator();

  // Reset the stationarity estimator.
  void Reset();

  // Update the stationarity estimator.
  void Update(rtc::ArrayView<const float> spectrum,
              size_t block_number,
              bool first_block);  // JVP that first_block is just for debugging

  // Update the flag indicating whether this current frame is stationary. For
  // getting a more robust estimation, it looks at future and/or past frames.
  void UpdateFlagsStationaryAhead(size_t current_block_number,
                                  size_t num_lookahead);

  // Returns true if the current band is stationary.
  bool IsBandStationaryAhead(size_t k) const {
    return (stationary_flag_[k] && (hangover_[k] == 0));
  }

 private:
  // Returns the power of the stationary noise spectrum at a band.
  float GetStationarityPowerBand(size_t k) const { return noise_.PowerBand(k); }

  // Write into the slot the information about the current frame that
  // is needed for the stationarity detection.
  void WriteInfoFrameInSlot(size_t block_number,
                            rtc::ArrayView<const float> spectrum);

  // Get an estimation of the stationarity for the current band by looking
  // at the past/present/future available data.
  bool EstimateBandStationarity(const std::vector<size_t>& idx_lookahead,
                                const std::vector<size_t>& idx_lookback,
                                size_t k) const;

  // True if all bands at the current point are stationary.
  bool AreAllBandsStationary();

  // Update the hangover depending on the stationary status of the current
  // frame.
  void UpdateHangover();

  // Get the slots that contain past/present and future data.
  void GetSlotsAheadBack(std::vector<size_t>* idx_lookahead,
                         std::vector<size_t>* idx_lookback,
                         size_t current_block_number);

  // Smooth the stationary detection by looking at neighbour frequency bands.
  void SmoothStationaryPerFreq();

  class NoiseSpectrum {
   public:
    NoiseSpectrum();
    ~NoiseSpectrum();

    // Reset the noise power spectrum estimate state.
    void Reset();

    // Update the noise power spectrum with a new frame.
    void Update(rtc::ArrayView<const float> spectrum);

    // Get the noise estimation power spectrum.
    rtc::ArrayView<const float> Spectrum() const {
      return rtc::ArrayView<const float>(noise_spectrum_);
    }

    // Get the noise power spectrum at a certain band.
    float PowerBand(size_t band) const {
      RTC_DCHECK_LT(band, noise_spectrum_.size());
      return noise_spectrum_[band];
    }

   private:
    // Get the update coefficient to be used for the current frame.
    float GetAlpha() const;

    // Update the noise power spectrum at a certain band with a new frame.
    float UpdateBandBySmoothing(float power_band,
                                float power_band_noise,
                                float alpha) const;
    std::array<float, kFftLengthBy2Plus1> noise_spectrum_;
    size_t block_counter_;
  };

  class CircularBuffer {
   public:
    static constexpr int kCircularBufferSize = 16;
    CircularBuffer();
    bool IsBlockNumberAlreadyUpdated(size_t block_number) const;
    size_t GetSlotNumber(size_t block_number) const {
      return block_number & (kCircularBufferSize - 1);
    }
    size_t SetBlockNumberInSlot(size_t block_number);
    void SetElementProperties(float band_power, int slot, int band) {
      slots_[slot].SetElementProperties(band_power, band);
    }

    float GetPowerBand(size_t slot, size_t band) const {
      return slots_[slot].GetPowerBand(band);
    }

   private:
    class Element {
     public:
      Element();
      size_t GetBlockNumber() const { return block_number_; }
      void SetBlockNumber(size_t block_number) { block_number_ = block_number; }
      void SetElementProperties(float band_power, size_t band) {
        RTC_DCHECK_LT(band, power_spectrum_.size());
        power_spectrum_[band] = band_power;
      }
      float GetPowerBand(size_t band) const { return power_spectrum_[band]; }

     private:
      size_t block_number_;
      std::array<float, kFftLengthBy2Plus1> power_spectrum_;
    };
    std::array<Element, kCircularBufferSize> slots_;
  };

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  NoiseSpectrum noise_;
  std::array<int, kFftLengthBy2Plus1> hangover_;
  std::array<bool, kFftLengthBy2Plus1> stationary_flag_;
  CircularBuffer buffer_;

 public:
  static constexpr size_t kMaxNumLookahead =
      CircularBuffer::kCircularBufferSize - 2;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
