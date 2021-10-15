/*
 *  Copyright 2021 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_KEY_VALUE_CONFIG_H_
#define API_TEST_MOCK_KEY_VALUE_CONFIG_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/transport/webrtc_key_value_config.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockKeyValueConfig : public WebRtcKeyValueConfig {
 public:
  MOCK_METHOD(std::string, Lookup, (absl::string_view key), (const, override));
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_MOCK_KEY_VALUE_CONFIG_H_
