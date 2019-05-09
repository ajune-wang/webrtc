/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/echo_cancellation_impl.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"

namespace webrtc {

std::unique_ptr<EchoCancellationImpl> EchoCancellationImpl::Create() {
  return nullptr;
}

struct EchoCancellationImpl::StreamProperties {};

class EchoCancellationImpl::Canceller {};

EchoCancellationImpl::EchoCancellationImpl()
    : drift_compensation_enabled_(false),
      metrics_enabled_(false),
      suppression_level_(
          EchoCancellationImpl::SuppressionLevel::kLowSuppression),
      stream_drift_samples_(0),
      was_stream_drift_set_(false),
      stream_has_echo_(false),
      delay_logging_enabled_(false),
      extended_filter_enabled_(false),
      delay_agnostic_enabled_(false),
      enforce_zero_stream_delay_(false) {}
EchoCancellationImpl::~EchoCancellationImpl() = default;

void EchoCancellationImpl::ProcessRenderAudio(
    rtc::ArrayView<const float> packed_render_audio) {
  RTC_CHECK(false);
}
int EchoCancellationImpl::ProcessCaptureAudio(AudioBuffer* audio,
                                              int stream_delay_ms) {
  RTC_CHECK(false);
  return -1;
}

int EchoCancellationImpl::enable_drift_compensation(bool enable) {
  RTC_CHECK(false);
  return -1;
}
bool EchoCancellationImpl::is_drift_compensation_enabled() const {
  RTC_CHECK(false);
  return false;
}

void EchoCancellationImpl::set_stream_drift_samples(int drift) {
  RTC_CHECK(false);
}
int EchoCancellationImpl::stream_drift_samples() const {
  RTC_CHECK(false);
  return 0;
}
int EchoCancellationImpl::set_suppression_level(SuppressionLevel level) {
  RTC_CHECK(false);
  return 0;
}
EchoCancellationImpl::SuppressionLevel EchoCancellationImpl::suppression_level()
    const {
  RTC_CHECK(false);
  return EchoCancellationImpl::SuppressionLevel::kLowSuppression;
}

bool EchoCancellationImpl::stream_has_echo() const {
  RTC_CHECK(false);
  return false;
}

int EchoCancellationImpl::enable_metrics(bool enable) {
  RTC_CHECK(false);
  return -1;
}
bool EchoCancellationImpl::are_metrics_enabled() const {
  RTC_CHECK(false);
  return false;
}

int EchoCancellationImpl::GetMetrics(Metrics* metrics) {
  RTC_CHECK(false);
  return -1;
}
int EchoCancellationImpl::enable_delay_logging(bool enable) {
  RTC_CHECK(false);
  return -1;
}
bool EchoCancellationImpl::is_delay_logging_enabled() const {
  RTC_CHECK(false);
  return false;
}
int EchoCancellationImpl::GetDelayMetrics(int* median, int* std) {
  RTC_CHECK(false);
  return -1;
}
int EchoCancellationImpl::GetDelayMetrics(int* median,
                                          int* std,
                                          float* fraction_poor_delays) {
  RTC_CHECK(false);
  return -1;
}
struct AecCore* EchoCancellationImpl::aec_core() const {
  RTC_CHECK(false);
  return nullptr;
}

void EchoCancellationImpl::Initialize(int sample_rate_hz,
                                      size_t num_reverse_channels_,
                                      size_t num_output_channels_,
                                      size_t num_proc_channels_) {
  RTC_CHECK(false);
}
void EchoCancellationImpl::SetExtraOptions(bool use_extended_filter,
                                           bool use_delay_agnostic,
                                           bool use_refined_adaptive_filter) {
  RTC_CHECK(false);
}
bool EchoCancellationImpl::is_delay_agnostic_enabled() const {
  RTC_CHECK(false);
  return false;
}
bool EchoCancellationImpl::is_extended_filter_enabled() const {
  RTC_CHECK(false);
  return false;
}
std::string EchoCancellationImpl::GetExperimentsDescription() {
  RTC_CHECK(false);
  return "";
}
bool EchoCancellationImpl::is_refined_adaptive_filter_enabled() const {
  RTC_CHECK(false);
  return false;
}
int EchoCancellationImpl::GetSystemDelayInSamples() const {
  RTC_CHECK(false);
  return 0;
}

void EchoCancellationImpl::PackRenderAudioBuffer(
    const AudioBuffer* audio,
    size_t num_output_channels,
    size_t num_channels,
    std::vector<float>* packed_buffer) {
  RTC_CHECK(false);
}
size_t EchoCancellationImpl::NumCancellersRequired(
    size_t num_output_channels,
    size_t num_reverse_channels) {
  RTC_CHECK(false);
  return 0;
}

}  // namespace webrtc
