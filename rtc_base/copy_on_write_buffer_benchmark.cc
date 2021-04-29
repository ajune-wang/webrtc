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

namespace rtc {
void BM_ModifyImmutableWithoutCopies(benchmark::State& state) {
  for (auto s : state) {
    RTC_UNUSED(s);
    CopyOnWriteBuffer mpb1("Hello World");
    mpb1 = mpb1.Slice(2, mpb1.size() - 2);

    CopyOnWriteBuffer pb2(std::move(mpb1));
    CopyOnWriteBuffer mpb2(std::move(pb2));
    mpb2.SetSize(mpb2.size() - 1);

    CopyOnWriteBuffer pb3(std::move(mpb2));
    CopyOnWriteBuffer mpb3(std::move(pb3));
    mpb3.SetSize(10);

    CopyOnWriteBuffer pb4(std::move(mpb3));
    CopyOnWriteBuffer mpb4(std::move(pb4));

    CopyOnWriteBuffer pb5(std::move(mpb4));
    CopyOnWriteBuffer mpb5(std::move(pb5));

    mpb5 = mpb1.Slice(2, mpb5.size() - 2);
    benchmark::DoNotOptimize(mpb5);
  }
}
BENCHMARK(BM_ModifyImmutableWithoutCopies);
}  // namespace rtc
