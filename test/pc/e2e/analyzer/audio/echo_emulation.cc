/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/audio/echo_emulation.h"

#include <utility>

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kSingleBufferDurationMs = 10;

}  // namespace

EchoEmulatingCapturer::EchoEmulatingCapturer(
    std::unique_ptr<TestAudioDeviceModule::Capturer> capturer,
    PeerConnectionE2EQualityTestFixture::EchoEmulationConfig config)
    : delegate_(std::move(capturer)), config_(config) {}

void EchoEmulatingCapturer::OnAudioRendered(
    rtc::ArrayView<const int16_t> data) {
  rtc::CritScope crit(&lock_);
  renderer_output_.push_back(std::vector<int16_t>(data.begin(), data.end()));
}

bool EchoEmulatingCapturer::Capture(rtc::BufferT<int16_t>* buffer) {
  bool result = delegate_->Capture(buffer);
  // Now we have to reduce input signal to make it possible safely mix in the
  // fake echo.
  for (size_t i = 0; i < buffer->size(); ++i) {
    (*buffer)[i] /= 2;
  }
  {
    rtc::CritScope crit(&lock_);
    if (renderer_output_.size() >=
        static_cast<size_t>(config_.echo_delay.ms() /
                            kSingleBufferDurationMs)) {
      std::vector<int16_t> echo = renderer_output_.front();
      renderer_output_.pop_front();
      for (size_t i = 0; i < buffer->size() && i < echo.size(); ++i) {
        (*buffer)[i] += echo[i];
      }
    }
  }
  return result;
}

EchoEmulatingRenderer::EchoEmulatingRenderer(
    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer,
    EchoEmulatingCapturer* echo_emulating_capturer)
    : delegate_(std::move(renderer)),
      echo_emulating_capturer_(echo_emulating_capturer) {}

bool EchoEmulatingRenderer::Render(rtc::ArrayView<const int16_t> data) {
  echo_emulating_capturer_->OnAudioRendered(data);
  return delegate_->Render(data);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
