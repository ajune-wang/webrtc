/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/aligned_malloc.h"

#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdint.h>
#endif

#include "test/gtest.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

// Returns true if |size| and |alignment| are valid combinations.
bool CorrectUsage(size_t size, size_t alignment) {
  std::unique_ptr<char, AlignedFreeDeleter> scoped(
      static_cast<char*>(AlignedMalloc(size, alignment)));
  if (scoped.get() == NULL) {
    return false;
  }
  const uintptr_t scoped_address = reinterpret_cast<uintptr_t>(scoped.get());
  return 0u == scoped_address % alignment;
}

TEST(AlignedMalloc, IncorrectSize) {
  const size_t incorrect_size = 0;
  const size_t alignment = 64;
  EXPECT_FALSE(CorrectUsage(incorrect_size, alignment));
}

TEST(AlignedMalloc, IncorrectAlignment) {
  const size_t size = 100;
  const size_t incorrect_alignment = 63;
  EXPECT_FALSE(CorrectUsage(size, incorrect_alignment));
}

TEST(AlignedMalloc, AlignTo2Bytes) {
  size_t size = 100;
  size_t alignment = 2;
  EXPECT_TRUE(CorrectUsage(size, alignment));
}

TEST(AlignedMalloc, AlignTo32Bytes) {
  size_t size = 100;
  size_t alignment = 32;
  EXPECT_TRUE(CorrectUsage(size, alignment));
}

TEST(AlignedMalloc, AlignTo128Bytes) {
  size_t size = 100;
  size_t alignment = 128;
  EXPECT_TRUE(CorrectUsage(size, alignment));
}

}  // namespace webrtc
