/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NUMERICS_TIME_SERIES_SET_EXPORTER_CREATE_H_
#define API_NUMERICS_TIME_SERIES_SET_EXPORTER_CREATE_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "api/numerics/time_series_set_exporter.h"

namespace webrtc {

std::unique_ptr<TimeSeriesSetExporter> CreateTimeSeriesSetExporter(
    absl::string_view name);

}  // namespace webrtc

#endif  // API_NUMERICS_TIME_SERIES_SET_EXPORTER_CREATE_H_
