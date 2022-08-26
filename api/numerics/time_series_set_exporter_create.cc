/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/numerics/time_series_set_exporter_create.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "api/numerics/time_series_set_exporter.h"
#include "rtc_base/numerics/time_series_set_exporter_impl.h"

namespace webrtc {

std::unique_ptr<TimeSeriesSetExporter> CreateTimeSeriesSetExporter(
    absl::string_view name) {
  return std::make_unique<TimeSeriesSetExporterImpl>(name);
}

}  // namespace webrtc
