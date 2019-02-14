/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_

#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
namespace webrtc {
// This class encapsulates the logic that controls how allocation of audio
// bitrate is done. This is primarily based on field trials, but also on the
// values of audio parameters.
class AudioAllocationSettings {
 public:
  AudioAllocationSettings();
  ~AudioAllocationSettings();
  // Returns true if the bitrate allocation range should be configured.
  bool ConfigureRateAllocationRange() const;
  // Returns true if audio packets should have transport wide sequence numbers
  // added to the packets if the extension has been negotiated.
  bool SendTransportSequenceNumber() const;
  // Returns true if audio should be added to rate allocation when the audio
  // stream is started.
  bool AlwaysIncludeAudioInAllocation() const;
  // Used for audio only calls to connect the congestion controller to RTCP
  // packets. Not required for video calls since the video stream will do the
  // same.
  bool RegisterRtcpObserver() const;
  // Returns true if AudioSendStream should signal to transport controller to
  // enable probing in Application Limited Regions.
  bool EnableAlrProbing() const;
  // Returns the min bitrate for audio rate allocation, excluding overhead.
  absl::optional<DataRate> DefaultMinBitrate() const;
  // Returns the max bitrate for audio rate allocation, excluding overhead.
  absl::optional<DataRate> DefaultMaxBitrate() const;
  // Indicates that legacy frame length values should be used instead of
  // accurate values in overhead calculations.
  bool UseLegacyFrameLengthForOverhead() const;
  // Returns the min bitrate for audio rate allocation, potentially including
  // overhead.
  int MinBitrateBps() const;
  // Returns the max bitrate for audio rate allocation, potentially including
  // overhead. |rtp_parameter_max_bitrate_bps| max bitrate as configured in rtp
  // parameters, excluding overhead.
  int MaxBitrateBps(absl::optional<int> rtp_parameter_max_bitrate_bps) const;
  // Indicates the default priority bitrate for audio streams. The bitrate
  // allocator will prioritize audio until it reaches this bitrate and will
  // divide bitrate evently between audio and video above this bitrate.
  DataRate DefaultPriorityBitrate() const;

 private:
  const bool legacy_audio_send_side_bwe_trial_;
  const bool legacy_allocate_audio_without_feedback_trial_;
  const bool legacy_audio_only_call_;
  const bool register_rtcp_observer_;
  const bool enable_alr_probing_;
  const bool send_transport_sequence_numbers_;
  const bool include_in_acknowledged_estimate_;
  const bool send_side_bwe_with_overhead_;
  int min_overhead_bps_ = 0;
  // Default bitrates to use as range if there's no user configured
  // bitrate range but audio bitrate allocation is enabled.
  FieldTrialParameter<DataRate> default_min_bitrate_;
  FieldTrialParameter<DataRate> default_max_bitrate_;
  FieldTrialParameter<DataRate> priority_bitrate_;
};
}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_
