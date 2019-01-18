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
class AudioAllocationSettings {
 public:
  AudioAllocationSettings();
  ~AudioAllocationSettings();
  bool force_no_audio_feedback() const;
  bool include_audio_in_feedback() const;
  bool ignore_seq_num_id_change() const;
  bool allocate_audio() const;
  bool include_audio_in_feedback(bool has_transport_sequence_number) const;

  bool update_audio_target_bitrate(bool has_transport_sequence_number_id) const;

  bool include_audio_in_allocation_on_start(
      bool min_rate_set,
      bool max_rate_set,
      bool has_dscp,
      bool has_transport_sequence_number_id) const;

  bool include_audio_in_allocation_on_reconfigure(
      bool min_rate_set,
      bool max_rate_set,
      bool has_dscp,
      bool has_transport_sequence_number_id) const;

  int min_bitrate_bps() const;
  int max_bitrate_bps(absl::optional<int> max_override_bps) const;

 private:
  FieldTrialFlag audio_send_side_bwe_;
  FieldTrialFlag allocate_audio_without_feedback_;
  FieldTrialFlag force_no_audio_feedback_;
  FieldTrialFlag audio_feedback_to_improve_video_bwe_;
  FieldTrialFlag send_side_bwe_with_overhead_;
  int min_overhead_bps_ = 0;
};
}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_
