/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_WIN)
#include <windows.h>  // Must come first.
#include <mmsystem.h>
#endif

#include <map>
#include <memory>
#include <vector>

#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/test/create_frame_generator.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

TEST(DefaultVideoQualityAnalyzerTest, CpuUsage) {
  // Set timer accuracy to 1 ms.
//#if defined(WEBRTC_WIN)
  timeBeginPeriod(1);
//#endif
  auto time1 = rtc::GetProcessCpuTimeNanos();

  float number = 1.5;
  for (size_t i = 0; i < 10000; ++i) {
    number += number / i;
    // RTC_LOG(INFO) << "Iteration number: " << i << " ; number = " << number;
  }
  RTC_LOG(INFO) << "Iteration number: " << number;
  auto time2 = rtc::GetProcessCpuTimeNanos();

  EXPECT_NE(time1, time2);
  EXPECT_NE(time2, 0);

//#if defined(WEBRTC_WIN)
  timeEndPeriod(1);
//#endif
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
