/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_SVC_SCALABILITY_MODE_UTIL_H_
#define MODULES_VIDEO_CODING_SVC_SCALABILITY_MODE_UTIL_H_

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"

namespace webrtc {

enum class ScalabilityModeResolutionRatio {
  kTwoToOne,    // The resolution ratio between spatial layers is 2:1.
  kThreeToTwo,  // The resolution ratio between spatial layers is 1.5:1.
};

static constexpr char kDefaultScalabilityModeStr[] = "L1T2";

absl::optional<ScalabilityMode> MakeScalabilityMode(
    int num_spatial_layers,
    int num_temporal_layers,
    InterLayerPredMode inter_layer_pred = InterLayerPredMode::kOff,
    absl::optional<ScalabilityModeResolutionRatio> ratio = absl::nullopt,
    bool shift = false);

absl::optional<ScalabilityMode> ScalabilityModeFromString(
    absl::string_view scalability_mode_string);

constexpr InterLayerPredMode ScalabilityModeToInterLayerPredMode(
    ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
    case ScalabilityMode::kL1T2:
    case ScalabilityMode::kL1T3:
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL2T1_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL2T3_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL3T1_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL3T2_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL3T3_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS2T1h:
    case ScalabilityMode::kS2T2:
    case ScalabilityMode::kS2T2h:
    case ScalabilityMode::kS2T3:
    case ScalabilityMode::kS2T3h:
    case ScalabilityMode::kS3T1:
    case ScalabilityMode::kS3T1h:
    case ScalabilityMode::kS3T2:
    case ScalabilityMode::kS3T2h:
    case ScalabilityMode::kS3T3:
    case ScalabilityMode::kS3T3h:
      return InterLayerPredMode::kOff;
  }
  RTC_CHECK_NOTREACHED();
}

constexpr int ScalabilityModeToNumSpatialLayers(
    ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
    case ScalabilityMode::kL1T2:
    case ScalabilityMode::kL1T3:
      return 1;
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1h:
    case ScalabilityMode::kL2T1_KEY:
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2h:
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3h:
    case ScalabilityMode::kL2T3_KEY:
      return 2;
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1h:
    case ScalabilityMode::kL3T1_KEY:
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2h:
    case ScalabilityMode::kL3T2_KEY:
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3h:
    case ScalabilityMode::kL3T3_KEY:
      return 3;
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS2T1h:
    case ScalabilityMode::kS2T2:
    case ScalabilityMode::kS2T2h:
    case ScalabilityMode::kS2T3:
    case ScalabilityMode::kS2T3h:
      return 2;
    case ScalabilityMode::kS3T1:
    case ScalabilityMode::kS3T1h:
    case ScalabilityMode::kS3T2:
    case ScalabilityMode::kS3T2h:
    case ScalabilityMode::kS3T3:
    case ScalabilityMode::kS3T3h:
      return 3;
  }
  RTC_CHECK_NOTREACHED();
}

constexpr int ScalabilityModeToNumTemporalLayers(
    ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
      return 1;
    case ScalabilityMode::kL1T2:
      return 2;
    case ScalabilityMode::kL1T3:
      return 3;
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1h:
    case ScalabilityMode::kL2T1_KEY:
      return 1;
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2h:
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
      return 2;
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3h:
    case ScalabilityMode::kL2T3_KEY:
      return 3;
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1h:
    case ScalabilityMode::kL3T1_KEY:
      return 1;
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2h:
    case ScalabilityMode::kL3T2_KEY:
      return 2;
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3h:
    case ScalabilityMode::kL3T3_KEY:
      return 3;
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS2T1h:
    case ScalabilityMode::kS3T1:
    case ScalabilityMode::kS3T1h:
      return 1;
    case ScalabilityMode::kS2T2:
    case ScalabilityMode::kS2T2h:
    case ScalabilityMode::kS3T2:
    case ScalabilityMode::kS3T2h:
      return 2;
    case ScalabilityMode::kS2T3:
    case ScalabilityMode::kS2T3h:
    case ScalabilityMode::kS3T3:
    case ScalabilityMode::kS3T3h:
      return 3;
  }
  RTC_CHECK_NOTREACHED();
}

constexpr absl::optional<ScalabilityModeResolutionRatio>
ScalabilityModeToResolutionRatio(ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
    case ScalabilityMode::kL1T2:
    case ScalabilityMode::kL1T3:
      return absl::nullopt;
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1_KEY:
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3_KEY:
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1_KEY:
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2_KEY:
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3_KEY:
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS2T2:
    case ScalabilityMode::kS2T3:
    case ScalabilityMode::kS3T1:
    case ScalabilityMode::kS3T2:
    case ScalabilityMode::kS3T3:
      return ScalabilityModeResolutionRatio::kTwoToOne;
    case ScalabilityMode::kL2T1h:
    case ScalabilityMode::kL2T2h:
    case ScalabilityMode::kL2T3h:
    case ScalabilityMode::kL3T1h:
    case ScalabilityMode::kL3T2h:
    case ScalabilityMode::kL3T3h:
    case ScalabilityMode::kS2T1h:
    case ScalabilityMode::kS2T2h:
    case ScalabilityMode::kS2T3h:
    case ScalabilityMode::kS3T1h:
    case ScalabilityMode::kS3T2h:
    case ScalabilityMode::kS3T3h:
      return ScalabilityModeResolutionRatio::kThreeToTwo;
  }
  RTC_CHECK_NOTREACHED();
}

constexpr bool ScalabilityModeIsShiftMode(ScalabilityMode scalability_mode) {
  return scalability_mode == ScalabilityMode::kL2T2_KEY_SHIFT;
}

ScalabilityMode LimitNumSpatialLayers(ScalabilityMode scalability_mode,
                                      int max_spatial_layers);

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SVC_SCALABILITY_MODE_UTIL_H_
