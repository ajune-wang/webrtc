/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_buffer.h"

#include <string.h>
#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator_by_4.h"
#include "modules/audio_processing/aec3/fft_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/matrix_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace aec3 {
namespace {

constexpr int kBufferHeadroom =
    kAdaptiveFilterLength > kUnknownDelayRenderWindowSize
        ? kAdaptiveFilterLength
        : kUnknownDelayRenderWindowSize;

class ApiCallJitterBuffer {
 public:
  explicit ApiCallJitterBuffer(size_t num_bands) {
    buffer_.fill(std::vector<std::vector<float>>(
        num_bands, std::vector<float>(kBlockSize, 0.f)));
  }

  ~ApiCallJitterBuffer() = default;

  void Reset() {
    size_ = 0;
    last_insert_index_ = 0;
  }

  void Insert(const std::vector<std::vector<float>>& block) {
    RTC_DCHECK_LT(size_, buffer_.size());
    last_insert_index_ = (last_insert_index_ + 1) % buffer_.size();
    RTC_DCHECK_EQ(buffer_[last_insert_index_].size(), block.size());
    RTC_DCHECK_EQ(buffer_[last_insert_index_][0].size(), block[0].size());
    for (size_t k = 0; k < block.size(); ++k) {
      std::copy(block[k].begin(), block[k].end(),
                buffer_[last_insert_index_][k].begin());
    }
    ++size_;
  }

  void Remove(std::vector<std::vector<float>>* block) {
    RTC_DCHECK_LT(0, size_);
    --size_;
    const size_t extract_index =
        (last_insert_index_ - size_ + buffer_.size()) % buffer_.size();
    for (size_t k = 0; k < block->size(); ++k) {
      std::copy(buffer_[extract_index][k].begin(),
                buffer_[extract_index][k].end(), (*block)[k].begin());
    }
  }

  size_t Size() const { return size_; }
  bool Full() const { return size_ >= (buffer_.size()); }
  bool Empty() const { return size_ == 0; }

 private:
  std::array<std::vector<std::vector<float>>, kMaxApiCallsJitterBlocks> buffer_;
  size_t size_ = 0;
  int last_insert_index_ = 0;
};

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  explicit RenderDelayBufferImpl(size_t num_bands);
  ~RenderDelayBufferImpl() override;

  void Clear() override;
  void ResetAlignment() override;
  bool Insert(const std::vector<std::vector<float>>& block) override;
  BufferingEvent UpdateBuffers() override;
  void SetDelay(size_t delay) override;
  size_t Delay() const override { return delay_; }
  size_t MaxDelay() const override {
    return block_buffer_.buffer.size() - 1 - kBufferHeadroom;
  }
  size_t MaxApiJitter() const override { return max_api_call_jitter_; }
  const RenderBuffer& GetRenderBuffer() const override {
    return echo_remover_buffer_;
  }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return downsampled_render_buffer_;
  }

  const BufferStatistics& GetStatistics() const override { return stats_; }

 private:
  const Aec3Optimization optimization_;
  MatrixBuffer block_buffer_;
  FftBuffer fft_buffer_;
  size_t delay_;
  size_t max_api_call_jitter_ = 0;
  int render_surplus_ = 0;
  bool first_reset_has_occurred_ = false;
  RenderBuffer echo_remover_buffer_;
  DownsampledRenderBuffer downsampled_render_buffer_;
  DecimatorBy4 render_decimator_;
  ApiCallJitterBuffer api_call_jitter_buffer_;
  const std::vector<std::vector<float>> zero_block_;
  const Aec3Fft fft_;
  BufferStatistics stats_;
  size_t capture_call_counter_ = 0;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

RenderDelayBufferImpl::RenderDelayBufferImpl(size_t num_bands)
    : optimization_(DetectOptimization()),
      block_buffer_(kRenderDelayBufferSize, num_bands, kBlockSize),
      fft_buffer_(kRenderDelayBufferSize),
      delay_(kMinEchoPathDelayBlocks),
      echo_remover_buffer_(
          optimization_,
          num_bands,
          std::max(kUnknownDelayRenderWindowSize, kAdaptiveFilterLength),
          std::vector<size_t>(1, kAdaptiveFilterLength),
          &block_buffer_,
          &fft_buffer_),
      api_call_jitter_buffer_(num_bands),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)),
      fft_() {
  block_buffer_.Clear();
  fft_buffer_.Clear();

  block_buffer_.next_read_index =
      (block_buffer_.buffer.size() + block_buffer_.last_insert_index - delay_) %
      block_buffer_.buffer.size();

  RTC_DCHECK_EQ(block_buffer_.buffer.size(), fft_buffer_.buffer.size());
}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

void RenderDelayBufferImpl::Clear() {
  api_call_jitter_buffer_.Reset();
  block_buffer_.Clear();
  fft_buffer_.Clear();
  downsampled_render_buffer_.buffer.fill(0.f);

  block_buffer_.last_insert_index = 0;
  fft_buffer_.last_insert_index = 0;
  downsampled_render_buffer_.last_insert_index = 0;

  ResetAlignment();
  capture_call_counter_ = 0;
}

void RenderDelayBufferImpl::ResetAlignment() {
  delay_ = kMinEchoPathDelayBlocks;

  block_buffer_.next_read_index =
      (block_buffer_.buffer.size() + block_buffer_.last_insert_index - delay_) %
      block_buffer_.buffer.size();
  fft_buffer_.next_read_index =
      (fft_buffer_.buffer.size() + fft_buffer_.last_insert_index + delay_) %
      fft_buffer_.buffer.size();
  downsampled_render_buffer_.next_read_index =
      downsampled_render_buffer_.last_insert_index;

  render_surplus_ = 0;
  if (!first_reset_has_occurred_) {
    max_api_call_jitter_ = 0;
  }
  first_reset_has_occurred_ = true;
}

bool RenderDelayBufferImpl::Insert(
    const std::vector<std::vector<float>>& block) {
  RTC_DCHECK_EQ(block.size(), block_buffer_.buffer[0].size());
  RTC_DCHECK_EQ(block[0].size(), block_buffer_.buffer[0][0].size());

  if (api_call_jitter_buffer_.Full()) {
    // Report buffer overrun and let the caller handle the overrun.
    return false;
  }
  api_call_jitter_buffer_.Insert(block);

  return true;
}

RenderDelayBufferImpl::BufferingEvent RenderDelayBufferImpl::UpdateBuffers() {
  const auto increase_read = [&]() {
    auto& b = downsampled_render_buffer_;
    b.next_read_index =
        (b.next_read_index - kSubBlockSize + b.buffer.size()) % b.buffer.size();
    block_buffer_.next_read_index =
        (block_buffer_.next_read_index + 1) % block_buffer_.buffer.size();
    fft_buffer_.next_read_index =
        (fft_buffer_.buffer.size() + fft_buffer_.next_read_index - 1) %
        block_buffer_.buffer.size();

  };

  const auto increase_insert = [&]() {
    auto& b = downsampled_render_buffer_;
    b.last_insert_index =
        (b.last_insert_index - kSubBlockSize + b.buffer.size()) %
        b.buffer.size();
    block_buffer_.last_insert_index =
        (block_buffer_.last_insert_index + 1) % block_buffer_.buffer.size();
    fft_buffer_.last_insert_index =
        (fft_buffer_.buffer.size() + fft_buffer_.last_insert_index - 1) %
        fft_buffer_.buffer.size();
  };

  BufferingEvent event = BufferingEvent::kNone;

  const size_t render_blocks_available = api_call_jitter_buffer_.Size();
  if (max_api_call_jitter_ < render_blocks_available &&
      first_reset_has_occurred_) {
    max_api_call_jitter_ = render_blocks_available;
  }

  // Empty any render blocks that have arrived.
  render_surplus_ += render_blocks_available;
  for (size_t k = 0; k < render_blocks_available; ++k) {
    const size_t prev_insert_index_ = block_buffer_.last_insert_index;
    increase_insert();

    // Handle the render buffer overrun.
    if (((block_buffer_.last_insert_index + kBufferHeadroom) %
         block_buffer_.buffer.size()) == block_buffer_.next_read_index) {
      event = BufferingEvent::kRenderOverrun;
      increase_read();
    }

    api_call_jitter_buffer_.Remove(
        &block_buffer_.buffer[block_buffer_.last_insert_index]);

    std::array<float, kSubBlockSize> render_ds;
    render_decimator_.Decimate(
        block_buffer_.buffer[block_buffer_.last_insert_index][0], render_ds);
    std::copy(render_ds.rbegin(), render_ds.rend(),
              downsampled_render_buffer_.buffer.begin() +
                  downsampled_render_buffer_.last_insert_index);

    fft_.PaddedFft(block_buffer_.buffer[block_buffer_.last_insert_index][0],
                   block_buffer_.buffer[prev_insert_index_][0],
                   &fft_buffer_.buffer[fft_buffer_.last_insert_index]);
  }
  RTC_DCHECK_EQ(0, api_call_jitter_buffer_.Size());

  // Handle render buffer underrun.
  if (downsampled_render_buffer_.next_read_index ==
      downsampled_render_buffer_.last_insert_index) {
    stats_.AddUnderrun(capture_call_counter_);
    increase_insert();
    event = BufferingEvent::kRenderUnderrun;
  }
  RTC_DCHECK_NE(block_buffer_.next_read_index, block_buffer_.last_insert_index);

  --render_surplus_;
  increase_read();
  echo_remover_buffer_.Insert(
      block_buffer_.buffer[block_buffer_.next_read_index]);

  // size_t n = (next_read_index_ + 1)%buffer_.size();
  // size_t k1 = next_read_index_;
  // size_t k2 = fft_buffer_.next_read_index;
  // for (size_t k = 0; k <buffer_.size(); ++k) {
  //   fft_.PaddedFft(buffer_[k1][0], buffer_[n][0], &fft_buffer_.buffer[k2]);
  //   n = k1;
  //   k1 = (k1 + 1)%buffer_.size();
  //   k2 = (buffer_.size() + k2 - 1)%buffer_.size();
  // }

  if (render_surplus_ == kMaxApiCallsJitterBlocks) {
    stats_.AddSurplusOverflow(capture_call_counter_);
    event = BufferingEvent::kApiCallSkew;
    RTC_LOG(LS_WARNING) << "Api call skew detected at " << capture_call_counter_
                        << ".";
  }

  ++capture_call_counter_;

  return event;
}

void RenderDelayBufferImpl::SetDelay(size_t delay) {
  if (delay_ == delay) {
    RTC_NOTREACHED();
    return;
  }

  int delta_delay = static_cast<int>(delay_) - static_cast<int>(delay);
  delay_ = delay;
  if (delay_ > MaxDelay()) {
    delay_ = std::min(MaxDelay(), delay);
    RTC_NOTREACHED();
  }

  // Recompute the read indices according to the set delay.
  block_buffer_.next_read_index =
      (block_buffer_.buffer.size() + block_buffer_.next_read_index +
       delta_delay) %
      block_buffer_.buffer.size();
  fft_buffer_.next_read_index =
      (fft_buffer_.buffer.size() + fft_buffer_.next_read_index - delta_delay) %
      fft_buffer_.buffer.size();
}

}  // namespace

}  // namespace aec3

RenderDelayBuffer* RenderDelayBuffer::Create(size_t num_bands) {
  return new aec3::RenderDelayBufferImpl(num_bands);
}

}  // namespace webrtc
