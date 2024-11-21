/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CORRUPTION_DETECTION_FILTER_SETTINGS_H_
#define API_VIDEO_CORRUPTION_DETECTION_FILTER_SETTINGS_H_

#include <stdint.h>

struct CorruptionDetectionFilterSettings {
  double std_dev = 0.0;
  int luma_error_threshold = 0;
  int chroma_error_threshold = 0;
};

#endif  // API_VIDEO_CORRUPTION_DETECTION_FILTER_SETTINGS_H_
