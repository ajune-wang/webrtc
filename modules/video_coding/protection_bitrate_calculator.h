/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_
#define MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_

#include <list>
#include <memory>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/media_opt_util.h"
#include "rtc_base/criticalsection.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// ProtectionBitrateCalculator calculates how much of the allocated network
// capacity that can be used by an encoder and how much that
// is needed for redundant packets such as FEC and NACK. It uses an
// implementation of |VCMProtectionCallback| to set new FEC parameters and get
// the bitrate currently used for FEC and NACK.
// Usage:
// Setup by calling SetProtectionMethod and SetEncodingData.
// For each encoded image, call UpdateWithEncodedData.
// Each time the bandwidth estimate change, call SetTargetRates. SetTargetRates
// will return the bitrate that can be used by an encoder.
// A lock is used to protect internal states, so methods can be called on an
// arbitrary thread.
class ProtectionBitrateCalculator {
 public:
  virtual ~ProtectionBitrateCalculator() {}

  virtual void SetProtectionCallback(
      VCMProtectionCallback* protection_callback) = 0;
  virtual void SetProtectionMethod(bool enable_fec, bool enable_nack) = 0;

  // Informs media optimization of initial encoding state.
  virtual void SetEncodingData(size_t width,
                               size_t height,
                               size_t num_temporal_layers,
                               size_t max_payload_size) = 0;

  // Returns target rate for the encoder given the channel parameters.
  // Inputs:  estimated_bitrate_bps - the estimated network bitrate in bits/s.
  //          actual_framerate - encoder frame rate.
  //          fraction_lost - packet loss rate in % in the network.
  //          round_trip_time_ms - round trip time in milliseconds.
  virtual uint32_t SetTargetRates(uint32_t estimated_bitrate_bps,
                                  int actual_framerate,
                                  uint8_t fraction_lost,
                                  int64_t round_trip_time_ms) = 0;

  virtual uint32_t SetTargetRates(uint32_t estimated_bitrate_bps,
                                  int actual_framerate,
                                  std::vector<bool> loss_mask_vector,
                                  int64_t round_trip_time_ms) = 0;

  virtual bool UseLossMaskVector() = 0;

  // Informs of encoded output.
  virtual void UpdateWithEncodedData(const EncodedImage& encoded_image) = 0;
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_
