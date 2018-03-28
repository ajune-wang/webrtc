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

/**
 * This interface is a thin wrapper on top of a native C++ webrtc::AudioDeviceModule.
 *
 * <p>Note: This class is still under development and may change without notice.
 */
public interface AudioDeviceModule {
  /** Returns a C++ pointer to a webrtc::AudioDeviceModule */
  long getNativeAudioDeviceModulePointer();

  /** Release all resources. This class should not be used after this call. */
  void release();

  /** Controls muting/unmuting the speaker. */
  void setSpeakerMute(boolean mute);

  /** Controls muting/unmuting the microphone. */
  void setMicrophoneMute(boolean mute);
}
