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

import org.webrtc.audio.AudioManager;
import org.webrtc.audio.AudioRecord;
import org.webrtc.audio.AudioTrack;
import org.webrtc.audio.AudioUtils;

/**
 * Public API for Java audio methods.
 *
 * <p>Note: This class is still under development and may change without notice.
 */
public class AudioDeviceModule {
  public AudioDeviceModule() {}

  /* AudioManager */
  public static void setBlacklistDeviceForOpenSLESUsage(boolean enable) {
    AudioManager.setBlacklistDeviceForOpenSLESUsage(enable);
  }

  public static void setStereoInput(boolean enable) {
    AudioManager.setStereoInput(enable);
  }

  /* AudioRecord */
  // Audio recording error handler functions.
  public enum AudioRecordStartErrorCode {
    AUDIO_RECORD_START_EXCEPTION,
    AUDIO_RECORD_START_STATE_MISMATCH,
  }

  public static interface AudioRecordErrorCallback {
    void onAudioRecordInitError(String errorMessage);
    void onAudioRecordStartError(AudioRecordStartErrorCode errorCode, String errorMessage);
    void onAudioRecordError(String errorMessage);
  }

  public static void setErrorCallback(AudioRecordErrorCallback errorCallback) {
    AudioRecord.setErrorCallback(errorCallback);
  }

  /* AudioTrack */
  // Audio playout/track error handler functions.
  public enum AudioTrackStartErrorCode {
    AUDIO_TRACK_START_EXCEPTION,
    AUDIO_TRACK_START_STATE_MISMATCH,
  }
  public static interface AudioTrackErrorCallback {
    void onAudioTrackInitError(String errorMessage);
    void onAudioTrackStartError(AudioTrackStartErrorCode errorCode, String errorMessage);
    void onAudioTrackError(String errorMessage);
  }

  public static void setErrorCallback(AudioTrackErrorCallback errorCallback) {
    AudioTrack.setErrorCallback(errorCallback);
  }

  /* AudioUtils */
  public static void setWebRtcBasedAcousticEchoCanceler(boolean enable) {
    AudioUtils.setWebRtcBasedAcousticEchoCanceler(enable);
  }

  public static void setWebRtcBasedNoiseSuppressor(boolean enable) {
    AudioUtils.setWebRtcBasedNoiseSuppressor(enable);
  }

  // Returns true if the device supports an audio effect (AEC or NS).
  // Four conditions must be fulfilled if functions are to return true:
  // 1) the platform must support the built-in (HW) effect,
  // 2) explicit use (override) of a WebRTC based version must not be set,
  // 3) the device must not be blacklisted for use of the effect, and
  // 4) the UUID of the effect must be approved (some UUIDs can be excluded).
  public static boolean isAcousticEchoCancelerSupported() {
    return AudioEffects.canUseAcousticEchoCanceler();
  }
  public static boolean isNoiseSuppressorSupported() {
    return AudioEffects.canUseNoiseSuppressor();
  }

  // Call this method if the default handling of querying the native sample
  // rate shall be overridden. Can be useful on some devices where the
  // available Android APIs are known to return invalid results.
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  public static void setDefaultSampleRateHz(int sampleRateHz) {
    AudioUtils.setDefaultSampleRateHz(sampleRateHz);
  }

  /**
   * Contains audio sample information.
   */
  public static class AudioSamples {
    /** See {@link AudioRecord#getAudioFormat()} */
    private final int audioFormat;
    /** See {@link AudioRecord#getChannelCount()} */
    private final int channelCount;
    /** See {@link AudioRecord#getSampleRate()} */
    private final int sampleRate;

    private final byte[] data;

    public AudioSamples(int audioFormat, int channelCount, int sampleRate, byte[] data) {
      this.audioFormat = audioFormat;
      this.channelCount = channelCount;
      this.sampleRate = sampleRate;
      this.data = data;
    }

    public int getAudioFormat() {
      return audioFormat;
    }

    public int getChannelCount() {
      return channelCount;
    }

    public int getSampleRate() {
      return sampleRate;
    }

    public byte[] getData() {
      return data;
    }
  }

  /** Called when new audio samples are ready. This should only be set for debug purposes */
  public static interface SamplesReadyCallback {
    void onAudioRecordSamplesReady(AudioSamples samples);
  }

  public static void setOnAudioSamplesReady(SamplesReadyCallback callback) {
    AudioRecord.setOnAudioSamplesReady(callback);
  }
}
