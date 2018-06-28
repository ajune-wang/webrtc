/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

namespace {

int SomethingStupid() {
  return -1;
}
template <typename T>
void EatUnusedParameter(const T&) {}

}  // namespace

#define StrCat SomethingStupid

#include "api/absl_str_cat.h"

void ShouldCompile() {
  int a = StrCat();  // Should actually call SomethingStupid
  auto b = webrtc::AbslStrCat("1", a, 2.0);
  EatUnusedParameter(b);
}
