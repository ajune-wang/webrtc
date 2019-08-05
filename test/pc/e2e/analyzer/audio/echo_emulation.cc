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
    : delegate_(std::move(capturer)),
      config_(config),
      renderer_queue_(1000 +
                      2 * config_.echo_delay.ms() / kSingleBufferDurationMs),
      queue_input_(TestAudioDeviceModule::SamplesPerFrame(
                       delegate_->SamplingFrequency()) *
                   delegate_->NumChannels()),
      queue_output_(TestAudioDeviceModule::SamplesPerFrame(
                        delegate_->SamplingFrequency()) *
                    delegate_->NumChannels()) {}

void EchoEmulatingCapturer::OnAudioRendered(
    rtc::ArrayView<const int16_t> data) {
  // Because rendering can start before capturing in the beginning we can have a
  // set of empty audio data frames. So we will skip them and will start fill
  // the queue only after 1st non-empty audio data frame will arrive.
  bool is_empty = true;
  for (auto d : data) {
    if (d != 0)
      is_empty = false;
  }
  if (!recording_started_ && is_empty) {
    return;
  }
  recording_started_ = true;
  queue_input_.assign(data.begin(), data.end());
  std::printf("[%p] renderer_queue_.Size()=%lu; data.size()=%lu; is_empty=%d\n",
              this, renderer_queue_.Size(), data.size(), is_empty);
  if (!renderer_queue_.Insert(&queue_input_)) {
    // It should happen when we shutting down PC and capturing is already
    // stopped, but rendering is still ongoing.
  }
}

bool EchoEmulatingCapturer::Capture(rtc::BufferT<int16_t>* buffer) {
  bool result = delegate_->Capture(buffer);
  // Now we have to reduce input signal to make it possible safely mix in the
  // fake echo.
  for (size_t i = 0; i < buffer->size(); ++i) {
    (*buffer)[i] /= 2;
  }

  if (renderer_queue_.Size() >=
      static_cast<size_t>(config_.echo_delay.ms() / kSingleBufferDurationMs)) {
    RTC_CHECK(renderer_queue_.Remove(&queue_output_));
    for (size_t i = 0; i < buffer->size() && i < queue_output_.size(); ++i) {
      (*buffer)[i] += queue_output_[i];
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
  if (data.size() > 0) {
    echo_emulating_capturer_->OnAudioRendered(data);
  }
  return delegate_->Render(data);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
