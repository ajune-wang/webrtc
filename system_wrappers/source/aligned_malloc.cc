/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/aligned_malloc.h"

#include <stdlib.h>

namespace webrtc {

namespace {
// Alignment must be an integer power of two.
bool ValidAlignment(size_t alignment) {
  if (!alignment) {
    return false;
  }
  return (alignment & (alignment - 1)) == 0;
}
}

void* AlignedMalloc(size_t size, size_t alignment) {
  if (size == 0) {
    return NULL;
  }
  if (!ValidAlignment(alignment)) {
    return NULL;
  }

  // aligned_alloc is C11 and C++17, but not C++11. May nevertheless
  // work in practice with C++11 compilers and libraries.
  return aligned_alloc(alignment,
                       // size argument must be a multiple of the alignment.
                       alignment * ((size + alignment - 1) / alignment));
}

void AlignedFree(void* mem_block) {
  free (mem_block);
}

}  // namespace webrtc
