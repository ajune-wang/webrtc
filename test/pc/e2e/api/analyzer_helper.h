/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_API_ANALYZER_HELPER_H_
#define TEST_PC_E2E_API_ANALYZER_HELPER_H_

#include <map>
#include <string>

#include "rtc_base/critical_section.h"

namespace webrtc {
namespace test {

class AnalyzerHelper {
 public:
  void AddTrackToStreamMapping(const std::string& track_id,
                               const std::string& stream_label);
  const std::string& GetStreamLabelFromTrackId(
      const std::string track_id) const;

 private:
  rtc::CriticalSection lock_;
  std::map<std::string, std::string> track_to_stream_map_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_API_ANALYZER_HELPER_H_
