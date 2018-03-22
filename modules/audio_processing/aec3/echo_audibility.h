/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

class EchoAudibility {
 public:
  EchoAudibility();
  ~EchoAudibility();
  void Update(const RenderBuffer& render_buffer,
              size_t delay_blocks,
              const std::array<float, kBlockSize>& s);

  float ResidualEchoScaling() const { return residual_echo_scaling_; }
  size_t NumNonAudibleBlocks() const { return num_nonaudible_blocks_; }

 private:
  class Stationarity {
   public:
    enum class SignalType { kNonStationary, kStationary };

    Stationarity();
    ~Stationarity();

    // Classify the signal and update the signal statistics.
    bool Update(rtc::ArrayView<const float> spectrum);

    // Classify the signal without updating the signal statistics.
    bool Analyze(rtc::ArrayView<const float> spectrum) const;

    // Returns the power of the stationary noise spectrum;
    float StationaryPower() const { return noise_.Power(); }

   private:
    class NoiseSpectrum {
     public:
      NoiseSpectrum();
      ~NoiseSpectrum();
      void Update(rtc::ArrayView<const float> spectrum, bool first_update);

      rtc::ArrayView<const float> Spectrum() const {
        return rtc::ArrayView<const float>(noise_spectrum_);
      }

      float Power() const { return power_; }

     private:
      static int instance_count_;
      std::unique_ptr<ApmDataDumper> data_dumper_;
      std::array<float, kFftLengthBy2Plus1> noise_spectrum_;
      std::array<int, kFftLengthBy2Plus1> counters_;
      float power_;
    };

    NoiseSpectrum noise_;
    size_t block_counter_;
    size_t stationarity_counter = 3;
    bool stationarity_ = false;
  };

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  Stationarity stationarity_;
  std::vector<bool> inaudible_blocks_;
  size_t convergence_counter_ = 0;
  size_t num_nonaudible_blocks_ = 0;
  float residual_echo_scaling_ = 1.f;
  size_t low_farend_counter_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(EchoAudibility);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
