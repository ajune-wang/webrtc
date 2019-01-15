/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_ID_GENERATOR_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_ID_GENERATOR_H_

#include <atomic>

namespace webrtc {
namespace test {

template <typename T>
class IdGenerator {
 public:
  virtual ~IdGenerator() = default;

  virtual T GetNextId() = 0;
};

class IntIdGenerator : public IdGenerator<int> {
 public:
  explicit IntIdGenerator(int start_value);
  ~IntIdGenerator() override;

  int GetNextId() override;

 private:
  std::atomic<int> next_id_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_ID_GENERATOR_H_
