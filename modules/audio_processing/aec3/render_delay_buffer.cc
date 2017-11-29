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
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/fft_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/matrix_buffer.h"
#include "rtc_base/atomicops.h"
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
  ApiCallJitterBuffer(size_t size, size_t num_bands)
      : buffer_(size,
                std::vector<std::vector<float>>(
                    num_bands,
                    std::vector<float>(kBlockSize, 0.f))) {}

  ~ApiCallJitterBuffer() = default;

  void Reset() {
    size_ = 0;
    last_insert_ = 0;
  }

  void Insert(const std::vector<std::vector<float>>& block) {
    RTC_DCHECK_LT(size_, buffer_.size());
    last_insert_ = (last_insert_ + 1) % buffer_.size();
    RTC_DCHECK_EQ(buffer_[last_insert_].size(), block.size());
    RTC_DCHECK_EQ(buffer_[last_insert_][0].size(), block[0].size());
    for (size_t k = 0; k < block.size(); ++k) {
      std::copy(block[k].begin(), block[k].end(),
                buffer_[last_insert_][k].begin());
    }
    ++size_;
  }

  void Remove(std::vector<std::vector<float>>* block) {
    RTC_DCHECK_LT(0, size_);
    --size_;
    const size_t extract_index =
        (last_insert_ - size_ + buffer_.size()) % buffer_.size();
    for (size_t k = 0; k < block->size(); ++k) {
      std::copy(buffer_[extract_index][k].begin(),
                buffer_[extract_index][k].end(), (*block)[k].begin());
    }
  }

  size_t Size() const { return size_; }
  bool Full() const { return size_ >= buffer_.size(); }
  bool Empty() const { return size_ == 0; }

 private:
  std::vector<std::vector<std::vector<float>>> buffer_;
  size_t size_ = 0;
  int last_insert_ = 0;
};

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl(const EchoCanceller3Config& config, size_t num_bands);
  ~RenderDelayBufferImpl() override;

  void Clear() override;
  void ResetAlignment() override;
  bool Insert(const std::vector<std::vector<float>>& block) override;
  BufferingEvent UpdateBuffers() override;
  void SetDelay(size_t delay) override;
  size_t Delay() const override { return delay_; }
  size_t MaxDelay() const override {
    return blocks_.buffer.size() - 1 - kBufferHeadroom;
  }
  size_t MaxApiJitter() const override { return max_api_jitter_; }
  const RenderBuffer& GetRenderBuffer() const override {
    return echo_remover_buffer_;
  }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return ds_render_;
  }

  const BufferStatistics& GetStatistics() const override { return stats_; }

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  const size_t sub_block_size_;
  MatrixBuffer blocks_;
  VectorBuffer spectra_;
  FftBuffer ffts_;
  size_t delay_;
  size_t max_api_jitter_ = 0;
  int render_surplus_ = 0;
  bool first_reset_occurred_ = false;
  RenderBuffer echo_remover_buffer_;
  DownsampledRenderBuffer ds_render_;
  Decimator render_decimator_;
  ApiCallJitterBuffer api_jitter_buffer_;
  const std::vector<std::vector<float>> zero_block_;
  const Aec3Fft fft_;
  BufferStatistics stats_;
  size_t capture_call_counter_ = 0;
  std::vector<float> render_ds_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

int RenderDelayBufferImpl::instance_count_ = 0;

RenderDelayBufferImpl::RenderDelayBufferImpl(const EchoCanceller3Config& config,
                                             size_t num_bands)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      optimization_(DetectOptimization()),
      config_(config),
      sub_block_size_(config_.delay.down_sampling_factor > 0
                          ? kBlockSize / config_.delay.down_sampling_factor
                          : kBlockSize),
      blocks_(GetRenderDelayBufferSize(config_.delay.down_sampling_factor,
                                       config_.delay.num_filters),
              num_bands,
              kBlockSize),
      spectra_(blocks_.buffer.size(), kFftLengthBy2Plus1),
      ffts_(blocks_.buffer.size()),
      delay_(config_.delay.min_echo_path_delay_blocks),
      echo_remover_buffer_(kAdaptiveFilterLength, &blocks_, &spectra_, &ffts_),
      ds_render_(GetDownSampledBufferSize(config_.delay.down_sampling_factor,
                                          config_.delay.num_filters)),
      render_decimator_(config_.delay.down_sampling_factor),
      api_jitter_buffer_(config_.delay.api_call_jitter_blocks, num_bands),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)),
      fft_(),
      render_ds_(sub_block_size_, 0.f) {
  Clear();
  RTC_DCHECK_EQ(blocks_.buffer.size(), ffts_.buffer.size());
  RTC_DCHECK_EQ(spectra_.buffer.size(), ffts_.buffer.size());
}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

void RenderDelayBufferImpl::Clear() {
  api_jitter_buffer_.Reset();
  blocks_.Clear();
  spectra_.Clear();
  ffts_.Clear();
  std::fill(ds_render_.buffer.begin(), ds_render_.buffer.end(), 0.f);

  blocks_.last_insert = 0;
  spectra_.last_insert = 0;
  ffts_.last_insert = 0;
  ds_render_.last_insert = 0;

  ResetAlignment();
  capture_call_counter_ = 0;
}

void RenderDelayBufferImpl::ResetAlignment() {
  delay_ = config_.delay.min_echo_path_delay_blocks;
  const int initial_jitter_offset =
      std::min(config_.delay.api_call_jitter_blocks,
               config_.delay.min_echo_path_delay_blocks);

  auto& d = ds_render_;
  auto& b = blocks_;
  auto& s = spectra_;
  auto& f = ffts_;
  d.next_read = d.OffsetIndex(
      d.last_insert, static_cast<int>(initial_jitter_offset * sub_block_size_));
  b.next_read = b.OffsetIndex(
      b.last_insert, -static_cast<int>(delay_ + initial_jitter_offset));
  s.next_read = s.OffsetIndex(s.last_insert,
                              static_cast<int>(delay_ + initial_jitter_offset));
  f.next_read = f.OffsetIndex(f.last_insert,
                              static_cast<int>(delay_ + initial_jitter_offset));
  render_surplus_ = 0;
  if (!first_reset_occurred_) {
    max_api_jitter_ = 0;
  }
  first_reset_occurred_ = true;
}

bool RenderDelayBufferImpl::Insert(
    const std::vector<std::vector<float>>& block) {
  RTC_DCHECK_EQ(block.size(), blocks_.buffer[0].size());
  RTC_DCHECK_EQ(block[0].size(), blocks_.buffer[0][0].size());

  if (api_jitter_buffer_.Full()) {
    // Report buffer overrun and let the caller handle the overrun.
    return false;
  }
  api_jitter_buffer_.Insert(block);

  return true;
}

RenderDelayBufferImpl::BufferingEvent RenderDelayBufferImpl::UpdateBuffers() {
  const auto increase_read = [&]() {
    ds_render_.UpdateNextReadIndex(-static_cast<int>(sub_block_size_));
    blocks_.IncNextReadIndex();
    spectra_.DecNextReadIndex();
    ffts_.DecNextReadIndex();
  };

  const auto increase_insert = [&]() {
    ds_render_.UpdateLastInsertIndex(-static_cast<int>(sub_block_size_));
    blocks_.IncLastInsertIndex();
    spectra_.DecLastInsertIndex();
    ffts_.DecLastInsertIndex();
  };

  BufferingEvent event = BufferingEvent::kNone;

  const size_t render_blocks_available = api_jitter_buffer_.Size();
  if (max_api_jitter_ < render_blocks_available && first_reset_occurred_) {
    max_api_jitter_ = render_blocks_available;
  }

  // Empty any render blocks that have arrived.
  render_surplus_ += render_blocks_available;
  for (size_t k = 0; k < render_blocks_available; ++k) {
    const size_t prev_insert = blocks_.last_insert;
    increase_insert();

    // Handle the render buffer overrun.
    if (ds_render_.next_read == ds_render_.last_insert ||
        blocks_.next_read == blocks_.last_insert) {
      event = BufferingEvent::kRenderOverrun;
      increase_read();
    }

    api_jitter_buffer_.Remove(&blocks_.buffer[blocks_.last_insert]);

    render_decimator_.Decimate(blocks_.buffer[blocks_.last_insert][0],
                               render_ds_);
    std::copy(render_ds_.rbegin(), render_ds_.rend(),
              ds_render_.buffer.begin() + ds_render_.last_insert);

    fft_.PaddedFft(blocks_.buffer[blocks_.last_insert][0],
                   blocks_.buffer[prev_insert][0],
                   &ffts_.buffer[ffts_.last_insert]);

    ffts_.buffer[ffts_.last_insert].Spectrum(
        optimization_, spectra_.buffer[spectra_.last_insert]);
  }
  RTC_DCHECK_EQ(0, api_jitter_buffer_.Size());

  // Handle render buffer underrun.
  if (ds_render_.next_read == ds_render_.last_insert ||
      blocks_.next_read == blocks_.last_insert) {
    stats_.AddUnderrun(capture_call_counter_);
    increase_insert();
    event = BufferingEvent::kRenderUnderrun;
  }
  RTC_DCHECK_NE(blocks_.next_read, blocks_.last_insert);

  --render_surplus_;
  increase_read();
  echo_remover_buffer_.UpdateSpectralSum();

  if (render_surplus_ >=
      static_cast<int>(config_.delay.api_call_jitter_blocks)) {
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
  printf("New delay:%zu\n", delay_);
  if (delay_ > MaxDelay()) {
    delay_ = std::min(MaxDelay(), delay);
    printf("B:%zu\n", MaxDelay());
    RTC_NOTREACHED();
  }

  // Recompute the read indices according to the set delay.
  blocks_.UpdateNextReadIndex(delta_delay);
  spectra_.UpdateNextReadIndex(-delta_delay);
  ffts_.UpdateNextReadIndex(-delta_delay);
}

}  // namespace

}  // namespace aec3

RenderDelayBuffer* RenderDelayBuffer::Create(const EchoCanceller3Config& config,
                                             size_t num_bands) {
  return new aec3::RenderDelayBufferImpl(config, num_bands);
}

}  // namespace webrtc
