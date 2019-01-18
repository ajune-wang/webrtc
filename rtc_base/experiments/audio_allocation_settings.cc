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
// For SendSideBwe, Opus bitrate should be in the range between 6000 and 32000.
const int kOpusMinBitrateBps = 6000;
const int kOpusBitrateFbBps = 32000;
}  // namespace
AudioAllocationSettings::AudioAllocationSettings()
    : audio_send_side_bwe_("Enabled"),
      allocate_audio_without_feedback_("Enabled"),
      force_no_audio_feedback_("Enabled"),
      audio_feedback_to_improve_video_bwe_("Enabled"),
      send_side_bwe_with_overhead_("Enabled") {
  ParseFieldTrial({&audio_send_side_bwe_},
                  field_trial::FindFullName("WebRTC-Audio-SendSideBwe"));
  ParseFieldTrial({&allocate_audio_without_feedback_},
                  field_trial::FindFullName("WebRTC-Audio-ABWENoTWCC"));
  ParseFieldTrial({&force_no_audio_feedback_},
                  field_trial::FindFullName("WebRTC-Audio-ForceNoTWCC"));
  ParseFieldTrial(
      {&audio_feedback_to_improve_video_bwe_},
      field_trial::FindFullName("WebRTC-Audio-SendSideBwe-For-Video"));
  ParseFieldTrial({&send_side_bwe_with_overhead_},
                  field_trial::FindFullName("WebRTC-SendSideBwe-WithOverhead"));

  // TODO(mflodman): Keep testing this and set proper values.
  // Note: This is an early experiment currently only supported by Opus.
  if (send_side_bwe_with_overhead_) {
    const int max_packet_size_ms = WEBRTC_OPUS_SUPPORT_120MS_PTIME ? 120 : 60;

    // OverheadPerPacket = Ipv4(20B) + UDP(8B) + SRTP(10B) + RTP(12)
    constexpr int kOverheadPerPacket = 20 + 8 + 10 + 12;

    min_overhead_bps_ = kOverheadPerPacket * 8 * 1000 / max_packet_size_ms;
  }
}

AudioAllocationSettings::~AudioAllocationSettings() {}

bool AudioAllocationSettings::force_no_audio_feedback() const {
  return force_no_audio_feedback_;
}

bool AudioAllocationSettings::ignore_seq_num_id_change() const {
  return !audio_send_side_bwe_;
}

bool AudioAllocationSettings::configure_audio_bitrate_allocation() const {
  return audio_send_side_bwe_;
}

bool AudioAllocationSettings::enable_transport_sequence_number_extension()
    const {
  // TODO(srte): Update this to be more accurate.
  return audio_send_side_bwe_ && !allocate_audio_without_feedback_;
}

bool AudioAllocationSettings::include_audio_in_feedback(
    int transport_sequence_number_extension_id) const {
  if (force_no_audio_feedback_)
    return false;
  return transport_sequence_number_extension_id != 0;
}

bool AudioAllocationSettings::update_audio_target_bitrate(
    int transport_sequence_number_extension_id) const {
  // If other side does not support audio TWCC and WebRTC-Audio-ABWENoTWCC is
  // not enabled, do not update target audio bitrate if we are in
  // WebRTC-Audio-SendSideBwe-For-Video experiment
  if (allocate_audio_without_feedback_ ||
      transport_sequence_number_extension_id != 0)
    return true;
  if (audio_feedback_to_improve_video_bwe_)
    return false;
  return true;
}

bool AudioAllocationSettings::include_audio_in_allocation_on_start(
    int min_bitrate_bps,
    int max_bitrate_bps,
    bool has_dscp,
    int transport_sequence_number_extension_id) const {
  if (has_dscp || min_bitrate_bps == -1 || max_bitrate_bps == -1)
    return false;
  if (transport_sequence_number_extension_id != 0 && !force_no_audio_feedback_)
    return true;
  if (allocate_audio_without_feedback_)
    return true;
  if (audio_send_side_bwe_)
    return false;
  return true;
}

bool AudioAllocationSettings::include_audio_in_allocation_on_reconfigure(
    int min_bitrate_bps,
    int max_bitrate_bps,
    bool has_dscp,
    int transport_sequence_number_extension_id) const {
  // TODO(srte): Make this match include_audio_in_allocation_on_start.
  if (has_dscp || min_bitrate_bps == -1 || max_bitrate_bps == -1)
    return false;
  if (transport_sequence_number_extension_id != 0)
    return true;
  if (audio_send_side_bwe_)
    return false;
  return true;
}

int AudioAllocationSettings::min_bitrate_bps() const {
  return kOpusMinBitrateBps + min_overhead_bps_;
}

int AudioAllocationSettings::max_bitrate_bps(
    absl::optional<int> rtp_param_bps) const {
  // We assume that rtp_param_bps is
  // a hard limit on the payload bitrate, so we add min_overhead_bps to
  // it to ensure that, when overhead is deducted, the payload rate
  // never goes beyond the limit.
  // Note: this also means that if a higher overhead is forced, we
  // cannot reach the limit.
  // TODO(minyue): Reconsider this when the signaling to BWE is done
  // through a dedicated API.

  // This means that when RtpParameters is reset, we may change the
  // encoder's bit rate immediately (through ReconfigureAudioSendStream()),
  // meanwhile change the cap to the output of BWE.
  if (rtp_param_bps)
    return *rtp_param_bps + min_overhead_bps_;
  return kOpusBitrateFbBps + min_overhead_bps_;
}
}  // namespace webrtc
