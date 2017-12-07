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
namespace {

constexpr int kBufferHeadroom = kAdaptiveFilterLength;
constexpr int kGlitchHeadroom = 0;

class RenderDelayBufferImpl final : public RenderDelayBuffer {
 public:
  RenderDelayBufferImpl(const EchoCanceller3Config& config, size_t num_bands);
  ~RenderDelayBufferImpl() override;

  void Reset() override;
  BufferingEvent Insert(const std::vector<std::vector<float>>& block) override;
  BufferingEvent PrepareCaptureCall() override;
  bool SetDelay(size_t delay) override;
  rtc::Optional<size_t> Delay() const override { return delay_; }
  size_t MaxDelay() const override {
    return blocks_.buffer.size() - 1 - kBufferHeadroom;
  }
  const RenderBuffer& GetRenderBuffer() const override {
    return echo_remover_buffer_;
  }

  const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const override {
    return low_rate_;
  }

  bool CausalDelay() const override;

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const Aec3Optimization optimization_;
  const EchoCanceller3Config config_;
  const int sub_block_size_;
  MatrixBuffer blocks_;
  VectorBuffer spectra_;
  FftBuffer ffts_;
  rtc::Optional<size_t> delay_;
  rtc::Optional<int> internal_delay_;
  RenderBuffer echo_remover_buffer_;
  DownsampledRenderBuffer low_rate_;
  Decimator render_decimator_;
  const std::vector<std::vector<float>> zero_block_;
  const Aec3Fft fft_;
  std::vector<float> render_ds_;

  int DelayComparedToWrite(size_t delay) const;
  void ApplyDelay(int delay);
  void InsertBlock(const std::vector<std::vector<float>>& block,
                   int previous_write);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayBufferImpl);
};

void IncreaseWriteIndex(int sub_block_size,
                        MatrixBuffer* blocks,
                        VectorBuffer* spectra,
                        FftBuffer* ffts,
                        DownsampledRenderBuffer* low_rate) {
  low_rate->UpdateWriteIndex(-sub_block_size);
  blocks->IncWriteIndex();
  spectra->DecWriteIndex();
  ffts->DecWriteIndex();
}

void IncreaseReadIndex(const rtc::Optional<int>& delay,
                       int sub_block_size,
                       MatrixBuffer* blocks,
                       VectorBuffer* spectra,
                       FftBuffer* ffts,
                       DownsampledRenderBuffer* low_rate) {
  RTC_DCHECK_NE(low_rate->read, low_rate->write);
  low_rate->UpdateReadIndex(-sub_block_size);

  if (blocks->read != blocks->write) {
    blocks->IncReadIndex();
    spectra->DecReadIndex();
    ffts->DecReadIndex();
  } else {
    // Only allow underrun for blocks_ when the delay is not set.
    RTC_DCHECK(!delay);
  }
}

bool RenderOverrun(const MatrixBuffer& b, const DownsampledRenderBuffer& l) {
  return l.read == l.write || b.read == b.write;
}

bool RenderUnderrun(const rtc::Optional<int>& delay,
                    const MatrixBuffer& b,
                    const DownsampledRenderBuffer& l) {
  return l.read == l.write || (delay && b.read == b.write);
}

bool ApiCallSkew(const DownsampledRenderBuffer& l,
                 int sub_block_size,
                 int offset_blocks,
                 int skew_limit_blocks) {
  int latency = (l.buffer.size() + l.read - l.write) % l.buffer.size();
  int api_call_skew = abs(offset_blocks * sub_block_size - latency);
  int skew_limit = skew_limit_blocks * sub_block_size;
  return api_call_skew > skew_limit;
}

int RenderDelayBufferImpl::instance_count_ = 0;

RenderDelayBufferImpl::RenderDelayBufferImpl(const EchoCanceller3Config& config,
                                             size_t num_bands)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      optimization_(DetectOptimization()),
      config_(config),
      sub_block_size_(
          static_cast<int>(config.delay.down_sampling_factor > 0
                               ? kBlockSize / config.delay.down_sampling_factor
                               : kBlockSize)),
      blocks_(GetRenderDelayBufferSize(config.delay.down_sampling_factor,
                                       config.delay.num_filters),
              num_bands,
              kBlockSize),
      spectra_(blocks_.buffer.size(), kFftLengthBy2Plus1),
      ffts_(blocks_.buffer.size()),
      echo_remover_buffer_(kAdaptiveFilterLength, &blocks_, &spectra_, &ffts_),
      low_rate_(GetDownSampledBufferSize(config.delay.down_sampling_factor,
                                         config.delay.num_filters)),
      render_decimator_(config.delay.down_sampling_factor),
      zero_block_(num_bands, std::vector<float>(kBlockSize, 0.f)),
      fft_(),
      render_ds_(sub_block_size_, 0.f) {
  RTC_DCHECK_EQ(blocks_.buffer.size(), ffts_.buffer.size());
  RTC_DCHECK_EQ(spectra_.buffer.size(), ffts_.buffer.size());
  Reset();
}

RenderDelayBufferImpl::~RenderDelayBufferImpl() = default;

void RenderDelayBufferImpl::Reset() {
  low_rate_.read = low_rate_.OffsetIndex(
      low_rate_.write, NonCausalRenderOffset(config_) * sub_block_size_);
  ApplyDelay(config_.delay.default_delay);
  delay_ = rtc::nullopt;
  internal_delay_ = rtc::nullopt;
}

RenderDelayBuffer::BufferingEvent RenderDelayBufferImpl::Insert(
    const std::vector<std::vector<float>>& block) {
  const int previous_write = blocks_.write;
  IncreaseWriteIndex(sub_block_size_, &blocks_, &spectra_, &ffts_, &low_rate_);

  // Allow overrun and do a reset when render overrun occurrs due to more render
  // data being inserted than capture data is received.
  BufferingEvent event = RenderOverrun(blocks_, low_rate_)
                             ? event = BufferingEvent::kRenderOverrun
                             : BufferingEvent::kNone;

  InsertBlock(block, previous_write);

  if (event != BufferingEvent::kNone) {
    Reset();
  }

  return event;
}

RenderDelayBuffer::BufferingEvent RenderDelayBufferImpl::PrepareCaptureCall() {
  BufferingEvent event = BufferingEvent::kNone;
  if (RenderUnderrun(internal_delay_, blocks_, low_rate_)) {
    event = BufferingEvent::kRenderUnderrun;
  } else {
    IncreaseReadIndex(internal_delay_, sub_block_size_, &blocks_, &spectra_,
                      &ffts_, &low_rate_);

    bool skew =
        ApiCallSkew(low_rate_, sub_block_size_, NonCausalRenderOffset(config_),
                    config_.delay.api_call_jitter_blocks);
    event = skew ? BufferingEvent::kApiCallSkew : BufferingEvent::kNone;
  }

  if (event != BufferingEvent::kNone) {
    Reset();
  }

  echo_remover_buffer_.UpdateSpectralSum();

  return event;
}

bool RenderDelayBufferImpl::SetDelay(size_t delay) {
  if (delay_ && *delay_ == delay) {
    return false;
  }
  delay_ = delay;
  int delay_compared_to_write = DelayComparedToWrite(*delay_);
  internal_delay_ = std::min(
      MaxDelay(), static_cast<size_t>(std::max(delay_compared_to_write, 0)));

  ApplyDelay(*internal_delay_);
  echo_remover_buffer_.UpdateSpectralSum();
  return true;
}

bool RenderDelayBufferImpl::CausalDelay() const {
  return !internal_delay_ ||
         *internal_delay_ >=
             static_cast<int>(config_.delay.min_echo_path_delay_blocks);
}

int RenderDelayBufferImpl::DelayComparedToWrite(size_t delay_blocks) const {
  auto& l = low_rate_;
  int delay_relative_read =
      static_cast<int>(delay_blocks) - NonCausalRenderOffset(config_);
  int latency = (l.buffer.size() + l.read - l.write) % l.buffer.size();
  RTC_DCHECK_LT(0, sub_block_size_);
  RTC_DCHECK_EQ(0, latency % sub_block_size_);
  int latency_blocks = latency / sub_block_size_;
  return latency_blocks + delay_relative_read;
}

void RenderDelayBufferImpl::ApplyDelay(int delay) {
  // Set the read indices according to the set delay.
  blocks_.read = blocks_.OffsetIndex(blocks_.write, -delay);
  spectra_.read = spectra_.OffsetIndex(spectra_.write, delay);
  ffts_.read = ffts_.OffsetIndex(ffts_.write, delay);
}

void RenderDelayBufferImpl::InsertBlock(
    const std::vector<std::vector<float>>& block,
    int previous_write) {
  auto& b = blocks_;
  auto& lr = low_rate_;
  auto& ds = render_ds_;
  auto& f = ffts_;
  auto& s = spectra_;
  RTC_DCHECK_EQ(block.size(), b.buffer[b.write].size());
  for (size_t k = 0; k < block.size(); ++k) {
    RTC_DCHECK_EQ(block[k].size(), b.buffer[b.write][k].size());
    std::copy(block[k].begin(), block[k].end(), b.buffer[b.write][k].begin());
  }

  render_decimator_.Decimate(block[0], ds);
  std::copy(ds.rbegin(), ds.rend(), lr.buffer.begin() + lr.write);
  fft_.PaddedFft(block[0], b.buffer[previous_write][0], &f.buffer[f.write]);
  f.buffer[f.write].Spectrum(optimization_, s.buffer[s.write]);
}

}  // namespace

int RenderDelayBuffer::RenderDelayBuffer::NonCausalRenderOffset(
    const EchoCanceller3Config& config) {
  return config.delay.api_call_jitter_blocks + kGlitchHeadroom;
}

RenderDelayBuffer* RenderDelayBuffer::Create(const EchoCanceller3Config& config,
                                             size_t num_bands) {
  return new RenderDelayBufferImpl(config, num_bands);
}

}  // namespace webrtc
