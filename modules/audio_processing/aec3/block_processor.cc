/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/block_processor.h"

#include "api/optional.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_processor_metrics.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

enum class BlockProcessorApiCall { kCapture, kRender };

class BlockProcessorImpl final : public BlockProcessor {
 public:
  BlockProcessorImpl(int sample_rate_hz,
                     std::unique_ptr<RenderDelayBuffer> render_buffer,
                     std::unique_ptr<RenderDelayController> delay_controller,
                     std::unique_ptr<EchoRemover> echo_remover);

  ~BlockProcessorImpl() override;

  void ProcessCapture(bool echo_path_gain_change,
                      bool capture_signal_saturation,
                      std::vector<std::vector<float>>* capture_block) override;

  void BufferRender(const std::vector<std::vector<float>>& block) override;

  void UpdateEchoLeakageStatus(bool leakage_detected) override;

 private:
  static int instance_count_;
  bool capture_properly_started_ = false;
  bool render_properly_started_ = false;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const size_t sample_rate_hz_;
  std::unique_ptr<RenderDelayBuffer> render_buffer_;
  std::unique_ptr<RenderDelayController> delay_controller_;
  std::unique_ptr<EchoRemover> echo_remover_;
  BlockProcessorMetrics metrics_;
  bool render_buffer_overrun_occurred_ = false;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(BlockProcessorImpl);
};

int BlockProcessorImpl::instance_count_ = 0;

BlockProcessorImpl::BlockProcessorImpl(
    int sample_rate_hz,
    std::unique_ptr<RenderDelayBuffer> render_buffer,
    std::unique_ptr<RenderDelayController> delay_controller,
    std::unique_ptr<EchoRemover> echo_remover)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      sample_rate_hz_(sample_rate_hz),
      render_buffer_(std::move(render_buffer)),
      delay_controller_(std::move(delay_controller)),
      echo_remover_(std::move(echo_remover)) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz_));
}

BlockProcessorImpl::~BlockProcessorImpl() = default;

void BlockProcessorImpl::ProcessCapture(
    bool echo_path_gain_change,
    bool capture_signal_saturation,
    std::vector<std::vector<float>>* capture_block) {
  RTC_DCHECK(capture_block);
  RTC_DCHECK_EQ(NumBandsForRate(sample_rate_hz_), capture_block->size());
  RTC_DCHECK_EQ(kBlockSize, (*capture_block)[0].size());
  data_dumper_->DumpRaw("aec3_processblock_call_order",
                        static_cast<int>(BlockProcessorApiCall::kCapture));
  data_dumper_->DumpWav("aec3_processblock_capture_input", kBlockSize,
                        &(*capture_block)[0][0],
                        LowestBandRate(sample_rate_hz_), 1);

  data_dumper_->DumpWav("aec3_processblock_capture_input2", kBlockSize,
                        &(*capture_block)[0][0],
                        LowestBandRate(sample_rate_hz_), 1);

  if (render_buffer_overrun_occurred_ && render_properly_started_) {
    // Reset the render buffers and the alignment functionality when there has
    // been a render buffer overrun as the buffer alignment may be noncausal.
    delay_controller_->Reset();
    render_buffer_->Clear();
    RTC_LOG(LS_WARNING)
        << "Hard reset due to unrecoverable render buffer overrun.";
  }

  // Update the render buffers with new render data, filling the buffers with
  // empty blocks when there is no render data available.
  const RenderDelayBuffer::BufferingEvent render_buffer_event =
      render_buffer_->UpdateBuffers();
  if (render_buffer_event ==
      RenderDelayBuffer::BufferingEvent::kRenderOverrun) {
    delay_controller_->Reset();
    render_buffer_->ResetAlignment();
    capture_properly_started_ = false;
    render_properly_started_ = false;
  } else if (render_buffer_event ==
                 RenderDelayBuffer::BufferingEvent::kRenderUnderrun ||
             render_buffer_event ==
                 RenderDelayBuffer::BufferingEvent::kApiCallSkew) {
    delay_controller_->Reset();
    render_buffer_->ResetAlignment();
    capture_properly_started_ = false;
    render_properly_started_ = false;
  }

  if (!capture_properly_started_ || !render_properly_started_) {
    capture_properly_started_ = true;
    render_buffer_->ResetAlignment();
  }

  // Compute and and apply the render delay required to achieve proper signal
  // alignment.
  const size_t estimated_delay = delay_controller_->GetDelay(
      render_buffer_->GetDownsampledRenderBuffer(), (*capture_block)[0]);
  const size_t new_delay =
      std::min(render_buffer_->MaxDelay(), estimated_delay);

  bool delay_change = render_buffer_->Delay() != new_delay;
  if (delay_change && new_delay >= kMinEchoPathDelayBlocks) {
    render_buffer_->SetDelay(new_delay);
    RTC_DCHECK_EQ(render_buffer_->Delay(), new_delay);
    delay_controller_->SetDelay(new_delay);
  } else if (delay_change && new_delay < kMinEchoPathDelayBlocks) {
    delay_controller_->Reset();
    render_buffer_->ResetAlignment();
    capture_properly_started_ = false;
    render_properly_started_ = false;
    RTC_LOG(LS_WARNING) << "Reset due to noncausal delay.";
  }

  // Remove the echo from the capture signal.
  echo_remover_->ProcessCapture(
      delay_controller_->AlignmentHeadroomSamples(),
      EchoPathVariability(echo_path_gain_change, delay_change),
      capture_signal_saturation, render_buffer_->GetRenderBuffer(),
      capture_block);

  // Update the metrics.
  metrics_.UpdateCapture(false);

  render_buffer_overrun_occurred_ = false;
}

void BlockProcessorImpl::BufferRender(
    const std::vector<std::vector<float>>& block) {
  RTC_DCHECK_EQ(NumBandsForRate(sample_rate_hz_), block.size());
  RTC_DCHECK_EQ(kBlockSize, block[0].size());
  data_dumper_->DumpRaw("aec3_processblock_call_order",
                        static_cast<int>(BlockProcessorApiCall::kRender));
  data_dumper_->DumpWav("aec3_processblock_render_input", kBlockSize,
                        &block[0][0], LowestBandRate(sample_rate_hz_), 1);

  data_dumper_->DumpWav("aec3_processblock_render_input2", kBlockSize,
                        &block[0][0], LowestBandRate(sample_rate_hz_), 1);

  // Buffer the render data.
  render_buffer_overrun_occurred_ = !render_buffer_->Insert(block);

  // Update the metrics.
  metrics_.UpdateRender(render_buffer_overrun_occurred_);

  if (capture_properly_started_) {
    render_properly_started_ = true;
  }
}

void BlockProcessorImpl::UpdateEchoLeakageStatus(bool leakage_detected) {
  echo_remover_->UpdateEchoLeakageStatus(leakage_detected);
}

}  // namespace

BlockProcessor* BlockProcessor::Create(const EchoCanceller3Config& config,
                                       int sample_rate_hz) {
  std::unique_ptr<RenderDelayBuffer> render_buffer(
      RenderDelayBuffer::Create(NumBandsForRate(sample_rate_hz)));
  std::unique_ptr<RenderDelayController> delay_controller(
      RenderDelayController::Create(config, sample_rate_hz));
  std::unique_ptr<EchoRemover> echo_remover(
      EchoRemover::Create(config, sample_rate_hz));
  return Create(config, sample_rate_hz, std::move(render_buffer),
                std::move(delay_controller), std::move(echo_remover));
}

BlockProcessor* BlockProcessor::Create(
    const EchoCanceller3Config& config,
    int sample_rate_hz,
    std::unique_ptr<RenderDelayBuffer> render_buffer) {
  std::unique_ptr<RenderDelayController> delay_controller(
      RenderDelayController::Create(config, sample_rate_hz));
  std::unique_ptr<EchoRemover> echo_remover(
      EchoRemover::Create(config, sample_rate_hz));
  return Create(config, sample_rate_hz, std::move(render_buffer),
                std::move(delay_controller), std::move(echo_remover));
}

BlockProcessor* BlockProcessor::Create(
    const EchoCanceller3Config& config,
    int sample_rate_hz,
    std::unique_ptr<RenderDelayBuffer> render_buffer,
    std::unique_ptr<RenderDelayController> delay_controller,
    std::unique_ptr<EchoRemover> echo_remover) {
  return new BlockProcessorImpl(sample_rate_hz, std::move(render_buffer),
                                std::move(delay_controller),
                                std::move(echo_remover));
}

}  // namespace webrtc
