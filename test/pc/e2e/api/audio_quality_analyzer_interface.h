/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_API_AUDIO_QUALITY_ANALYZER_INTERFACE_H_
#define TEST_PC_E2E_API_AUDIO_QUALITY_ANALYZER_INTERFACE_H_

#include <map>
#include <string>

#include "test/pc/e2e/api/analyzer_helper.h"
#include "test/pc/e2e/api/stats_observer_interface.h"

namespace webrtc {
namespace test {

class AudioQualityAnalyzerInterface : public StatsObserverInterface {
 public:
  ~AudioQualityAnalyzerInterface() override = default;

  // Will be called by framework before test.
  // |test_case_name| is name of test case, that should be used to report all
  // audio metrics.
  virtual void Start(std::string test_case_name,
                     AnalyzerHelper* analyzer_helper) = 0;

  // Will be called by the framework at the end of the test. The analyzer
  // has to finalize all its stats and it should report them.
  // TODO(mbonadei): Discuss about the API of this method. It is not clear
  // what does it mean for an analyzer to finalize its stats.
  // E.g. The current interpretation is that the analyzer will have to call
  // webrtc::test::PrintResult or similar in this method.
  virtual void Stop() = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_API_AUDIO_QUALITY_ANALYZER_INTERFACE_H_
