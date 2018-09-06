/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_FACTORY_H_
#define API_TEST_NETEQ_SIMULATOR_FACTORY_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "api/test/neteq_simulator.h"

namespace webrtc {
namespace test {

class NetEqTestFactory;

class NetEqSimulatorFactory {
 public:
  NetEqSimulatorFactory();
  ~NetEqSimulatorFactory();
  // Sets field trials. Note that this function should not be called more than
  // once.
  // A field_trial string may be passed in to set the field trials that should
  // be used. Field trials control experimental feature code which can be
  // forced. E.g. running with "WebRTC-FooFeature/Enable/" will enable the
  // field trial WebRTC-FooFeature.
  void SetFieldTrials(absl::string_view field_trials);
  // This function takes the same arguments as the neteq_rtpplay utility.
  std::unique_ptr<NetEqSimulator> CreateSimulator(int argc, char* argv[]);

 private:
  std::unique_ptr<NetEqTestFactory> factory_;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_NETEQ_SIMULATOR_FACTORY_H_
