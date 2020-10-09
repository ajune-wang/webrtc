/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "benchmark/benchmark.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {

static void BM_NV12Scale_libyuv(benchmark::State& state) {
  std::unique_ptr<test::FrameGeneratorInterface> gen =
      test::CreateSquareFrameGenerator(
          1280, 720, test::FrameGeneratorInterface::OutputType::kNV12,
          absl::nullopt);
  auto frame = gen->NextFrame();
  const auto* nv12 = frame.buffer->GetNV12();
  const int width = state.range(0);
  const int height = state.range(1);

  rtc::scoped_refptr<NV12Buffer> buffer = NV12Buffer::Create(width, height);
  for (auto _ : state) {
    libyuv::NV12Scale(
        nv12->DataY(), nv12->StrideY(), nv12->DataUV(), nv12->StrideUV(), 1280,
        720, buffer->MutableDataY(), buffer->StrideY(), buffer->MutableDataUV(),
        buffer->StrideUV(), width, height, libyuv::kFilterBilinear);
  }
}

static void BM_NV12Scale_video_common(benchmark::State& state) {
  std::unique_ptr<test::FrameGeneratorInterface> gen =
      test::CreateSquareFrameGenerator(
          1280, 720, test::FrameGeneratorInterface::OutputType::kNV12,
          absl::nullopt);
  auto frame = gen->NextFrame();
  const auto* nv12 = frame.buffer->GetNV12();
  const int width = state.range(0);
  const int height = state.range(1);

  rtc::scoped_refptr<NV12Buffer> buffer = NV12Buffer::Create(width, height);
  std::vector<uint8_t> tmp_buffer;
  tmp_buffer.resize(nv12->ChromaHeight() * nv12->ChromaWidth() * 2 +
                    buffer->ChromaHeight() * buffer->ChromaWidth() * 2);
  tmp_buffer.shrink_to_fit();
  for (auto _ : state) {
    NV12Scale(tmp_buffer.data(), nv12->DataY(), nv12->StrideY(), nv12->DataUV(),
              nv12->StrideUV(), 1280, 720, buffer->MutableDataY(),
              buffer->StrideY(), buffer->MutableDataUV(), buffer->StrideUV(),
              width, height);
  }
}

BENCHMARK(BM_NV12Scale_libyuv)
    ->Args({960, 540})
    ->Args({640, 360})
    ->Args({320, 180})
    ->Args({160, 90})
    ->Args({800, 450});

BENCHMARK(BM_NV12Scale_video_common)
    ->Args({960, 540})
    ->Args({640, 360})
    ->Args({320, 180})
    ->Args({160, 90})
    ->Args({800, 450});

}  // namespace webrtc
