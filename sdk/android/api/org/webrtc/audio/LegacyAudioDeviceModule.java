/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import org.webrtc.voiceengine.WebRtcAudioRecord;
import org.webrtc.voiceengine.WebRtcAudioTrack;

/**
 * Current AudioDeviceModule that is hardcoded into C++ WebRTC. This class will eventually be
 * removed. Use JavaAudioDeviceModule, OpenSLESAudioDeviceModule, or AAudioDeviceModule instead.
 */
@Deprecated
public class LegacyAudioDeviceModule implements AudioDeviceModule {
  public static AudioDeviceModule Create() {
    return new LegacyAudioDeviceModule();
  }

  @Override
  public long getNativeAudioDeviceModulePointer() {
    // Returning a null pointer will make WebRTC construct the built-in legacy AudioDeviceModule for
    // Android internally.
    return 0;
  }

  @Override
  public void release() {
    // All control for this ADM goes through static global methods and the C++ object is owned
    // internally by WebRTC.
  }

  @Override
  public void setSpeakerMute(boolean mute) {
    WebRtcAudioTrack.setSpeakerMute(mute);
  }

  @Override
  public void setMicrophoneMute(boolean mute) {
    WebRtcAudioRecord.setMicrophoneMute(mute);
  }
}
