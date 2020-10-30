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

// VoipVolumeControl interface
//
// This sub-API supports functions related to the input (microphone) and output
// (speaker) device.
class VoipVolumeControl {
 public:
  // Mutes or unmutes the microphone input signal completely without affecting
  // the audio device volume. Note that mute doesn't affect speech input
  // level and energy value as input sample is silenced after the measurement.
  virtual void SetInputMute(ChannelId channel_id, bool enable) = 0;

  // Returns the microphone speech level within the linear range [0,32767].
  // Note that this API's update frequency is 100 ms and caller should call at
  // most once every 100 ms.
  virtual int GetSpeechInputLevelFullRange(ChannelId channel_id) = 0;

  // Returns the speaker speech level within the linear range [0,32767].
  // Note that this API's update frequency is 100 ms and caller should call at
  // most once every 100 ms.
  virtual int GetSpeechOutputLevelFullRange(ChannelId channel_id) = 0;

  // Gets energy and duration of microphone.
  // (https://w3c.github.io/webrtc-stats/#dom-rtcaudiohandlerstats-totalaudioenergy)
  virtual void GetSpeechInputEnergyAndDuration(ChannelId channel_id,
                                               double& energy,
                                               double& duration) = 0;

  // Gets energy and duration of speaker.
  // (https://w3c.github.io/webrtc-stats/#dom-rtcaudiohandlerstats-totalaudioenergy)
  virtual void GetSpeechOutputEnergyAndDuration(ChannelId channel_id,
                                                double& energy,
                                                double& duration) = 0;

 protected:
  virtual ~VoipVolumeControl() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_VOLUME_CONTROL_H_
