/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/video/video_layers_allocation.h"
#include "modules/rtp_rtcp/source/rtp_video_layers_allocation_extension.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzDataHelper fuzz_data(rtc::MakeArrayView(data, size));
  while (fuzz_data.CanReadBytes(1)) {
    // Treat next byte as size of the next extension. That aligns how
    // two-byte rtp header extension sizes are written.
    size_t next_size = fuzz_data.Read<uint8_t>();
    auto raw =
        fuzz_data.ReadByteArray(std::min(next_size, fuzz_data.BytesLeft()));

    // Read the random input.
    VideoLayersAllocation allocation1;
    if (!RtpVideoLayersAllocationExtension::Parse(raw, &allocation1)) {
      // Ignore invalid buffer and move on.
      continue;
    }

    // Write parsed allocation back into raw buffer.
    size_t value_size =
        RtpVideoLayersAllocationExtension::ValueSize(allocation1);
    // Check `writer` use minimal number of bytes to pack the descriptor by
    // checking it doesn't use more than reader consumed.
    RTC_CHECK_LE(value_size, raw.size());
    uint8_t some_memory[256];
    // That should be true because value_size <= next_size < 256
    RTC_CHECK_LT(value_size, 256);
    rtc::ArrayView<uint8_t> write_buffer(some_memory, value_size);
    RTC_CHECK(
        RtpVideoLayersAllocationExtension::Write(write_buffer, allocation1));

    // Parse what Write assembled.
    // Unlike random input that should always succeed.
    VideoLayersAllocation allocation2;
    RTC_CHECK(
        RtpVideoLayersAllocationExtension::Parse(write_buffer, &allocation2));

    RTC_CHECK_EQ(allocation1.rtp_stream_index, allocation2.rtp_stream_index);
    RTC_CHECK_EQ(allocation1.resolution_and_frame_rate_is_valid,
                 allocation2.resolution_and_frame_rate_is_valid);
    RTC_CHECK_EQ(allocation1.active_spatial_layers.size(),
                 allocation2.active_spatial_layers.size());
    RTC_CHECK(allocation1 == allocation2);
  }
}

}  // namespace webrtc
