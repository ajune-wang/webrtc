/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "benchmark/benchmark.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/system/unused.h"

namespace rtc {
void BM_ModifyImmutableWithoutCopies(benchmark::State& state) {
  for (auto s : state) {
    RTC_UNUSED(s);
    CopyOnWriteBuffer cow1("Hello World");
    benchmark::DoNotOptimize(cow1);
    cow1 = cow1.Slice(2, cow1.size() - 2);
    benchmark::DoNotOptimize(cow1);

    CopyOnWriteBuffer cow2(std::move(cow1));
    benchmark::DoNotOptimize(cow2);
    CopyOnWriteBuffer cow3(std::move(cow2));
    benchmark::DoNotOptimize(cow3);
    cow3.SetSize(cow3.size() - 1);

    CopyOnWriteBuffer cow4(std::move(cow3));
    cow4.SetSize(10);
    benchmark::DoNotOptimize(cow4);

    CopyOnWriteBuffer cow5(std::move(cow4));
    benchmark::DoNotOptimize(cow5);

    cow5 = cow1.Slice(2, cow5.size() - 2);
    benchmark::DoNotOptimize(cow5);
  }
}
BENCHMARK(BM_ModifyImmutableWithoutCopies);
}  // namespace rtc
