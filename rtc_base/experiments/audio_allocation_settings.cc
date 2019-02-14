/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/audio_allocation_settings.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
// Based on min bitrate for Opus codec.
constexpr DataRate kDefaultMinEncoderBitrate = DataRate::KilobitsPerSec<6>();
constexpr DataRate kDefaultMaxEncoderBitrate = DataRate::KilobitsPerSec<32>();
constexpr int kOverheadPerPacket = 20 + 8 + 10 + 12;
}  // namespace

AudioAllocationSettings::AudioAllocationSettings()
    : legacy_audio_send_side_bwe_trial_(
          field_trial::IsEnabled("WebRTC-Audio-SendSideBwe")),
      legacy_allocate_audio_without_feedback_trial_(
          field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")),
      legacy_audio_only_call_(legacy_audio_send_side_bwe_trial_ &&
                              !legacy_allocate_audio_without_feedback_trial_),
      register_rtcp_observer_(
          field_trial::IsEnabled("WebRTC-Audio-RegisterRtcpObserver")),
      enable_alr_probing_(
          field_trial::IsEnabled("WebRTC-Audio-EnableAlrProbing")),
      send_transport_sequence_numbers_(
          field_trial::IsEnabled("WebRTC-Audio-SendTransportSequenceNumbers")),
      include_in_acknowledged_estimate_(
          field_trial::IsEnabled("WebRTC-Audio-AddSentToAckedEstimate")),
      send_side_bwe_with_overhead_(
          field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      default_min_bitrate_("min", kDefaultMinEncoderBitrate),
      default_max_bitrate_("max", kDefaultMaxEncoderBitrate),
      priority_bitrate_("prio", DataRate::Zero()) {
  ParseFieldTrial({&default_min_bitrate_, &default_max_bitrate_},
                  field_trial::FindFullName("WebRTC-Audio-Allocation"));
  // We can include audio in estimates by sending timestamps for it or by adding
  // sent audio to the acknowledged estimate, but doing both will make the
  // acknowledged estimate too high.
  RTC_DCHECK(
      !(include_in_acknowledged_estimate_ && send_transport_sequence_numbers_));
  // TODO(mflodman): Keep testing this and set proper values.
  // Note: This is an early experiment currently only supported by Opus.
  if (send_side_bwe_with_overhead_) {
    constexpr int kMaxPacketSizeMs = WEBRTC_OPUS_SUPPORT_120MS_PTIME ? 120 : 60;
    min_overhead_bps_ = kOverheadPerPacket * 8 * 1000 / kMaxPacketSizeMs;
  }
}

AudioAllocationSettings::~AudioAllocationSettings() {}

bool AudioAllocationSettings::SendTransportSequenceNumber() const {
  return legacy_audio_only_call_ || send_transport_sequence_numbers_;
}

bool AudioAllocationSettings::AlwaysIncludeAudioInAllocation() const {
  return legacy_allocate_audio_without_feedback_trial_ ||
         include_in_acknowledged_estimate_;
}

bool AudioAllocationSettings::ConfigureRateAllocationRange() const {
  return legacy_audio_send_side_bwe_trial_;
}

bool AudioAllocationSettings::RegisterRtcpObserver() const {
  return register_rtcp_observer_ || legacy_audio_only_call_;
}

bool AudioAllocationSettings::EnableAlrProbing() const {
  return enable_alr_probing_ || legacy_audio_only_call_;
}

bool AudioAllocationSettings::UseLegacyFrameLengthForOverhead() const {
  return legacy_audio_send_side_bwe_trial_;
}

int AudioAllocationSettings::MinBitrateBps() const {
  return default_min_bitrate_->bps() + min_overhead_bps_;
}

int AudioAllocationSettings::MaxBitrateBps(
    absl::optional<int> rtp_parameter_max_bitrate_bps) const {
  // We assume that the max is a hard limit on the payload bitrate, so we add
  // min_overhead_bps to it to ensure that, when overhead is deducted, the
  // payload rate never goes beyond the limit.  Note: this also means that if a
  // higher overhead is forced, we cannot reach the limit.
  // TODO(minyue): Reconsider this when the signaling to BWE is done
  // through a dedicated API.

  // This means that when RtpParameters is reset, we may change the
  // encoder's bit rate immediately (through ReconfigureAudioSendStream()),
  // meanwhile change the cap to the output of BWE.
  if (rtp_parameter_max_bitrate_bps)
    return *rtp_parameter_max_bitrate_bps + min_overhead_bps_;
  return default_max_bitrate_->bps() + min_overhead_bps_;
}

DataRate AudioAllocationSettings::DefaultPriorityBitrate() const {
  DataRate max_overhead = DataRate::Zero();
  if (send_side_bwe_with_overhead_) {
    const TimeDelta kMinPacketDuration = TimeDelta::ms(20);
    max_overhead = DataSize::bytes(kOverheadPerPacket) / kMinPacketDuration;
  }
  return priority_bitrate_.Get() + max_overhead;
}

}  // namespace webrtc
