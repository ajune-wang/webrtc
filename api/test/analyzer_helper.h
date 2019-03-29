/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_ANALYZER_HELPER_H_
#define API_TEST_ANALYZER_HELPER_H_

#include <map>
#include <string>

#include "rtc_base/critical_section.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// This class is a utility that provides bookkeeing capabilities that
// are useful to associate stats reports track_ids to the remote stream_id.
// An instance of this class is automatically populated by the framework
// and passed to the Start method of Media Quality Analyzers.
class AnalyzerHelper {
 public:
  void AddTrackToStreamMapping(const std::string& track_id,
                               const std::string& stream_label);

  // It returns a reference to a stream label owned by the AnalyzerHelper.
  // This functions expects the track_id to be mapped to a stream_label,
  // if that precondition is not true, the program will terminate.
  const std::string& GetStreamLabelFromTrackId(
      const std::string track_id) const;

 private:
  rtc::CriticalSection lock_;
  std::map<std::string, std::string> track_to_stream_map_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // API_TEST_ANALYZER_HELPER_H_
