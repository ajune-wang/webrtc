/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_VOLUME_CONTROL_H_
#define API_VOIP_VOIP_VOLUME_CONTROL_H_

#include "api/voip/voip_base.h"

namespace webrtc {

struct SpeechLevel {
  // Level indicated with the linear range between 0 and 32767.
  int level = 0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcaudiosourcestats-totalaudioenergy
  double energy = 0.0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcaudiosourcestats-totalsamplesduration
  double duration = 0.0;
};

// VoipVolumeControl interface
//
// This sub-API supports functions related to the input (microphone) and output
// (speaker) device.
//
// Caller must ensure that ChannelId is valid otherwise it will result in no-op
// and result in error logging.
class VoipVolumeControl {
 public:
  // Mutes or unmutes the microphone input signal completely without affecting
  // the audio device volume. Note that mute doesn't affect speech input
  // level and energy value as input sample is silenced after the measurement.
  virtual void SetInputMute(ChannelId channel_id, bool enable) = 0;

  // Returns the microphone speech level.
  virtual SpeechLevel GetSpeechInputLevel(ChannelId channel_id) = 0;

  // Returns the speaker speech level.
  virtual SpeechLevel GetSpeechOutputLevel(ChannelId channel_id) = 0;

 protected:
  virtual ~VoipVolumeControl() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_VOLUME_CONTROL_H_
