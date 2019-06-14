/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

// Explicit imports necessary for JNI generation.
import android.support.annotation.Nullable;
import java.nio.ByteBuffer;
import java.util.List;
import org.webrtc.VideoEncoder;

/**
 * This class contains the Java glue code for JNI generation of VideoEncoder.
 */
class VideoEncoderWrapper {
  @CalledByNative
  static boolean getScalingSettingsOn(VideoEncoder.ScalingSettings scalingSettings) {
    return scalingSettings.on;
  }

  @Nullable
  @CalledByNative
  static Integer getScalingSettingsLow(VideoEncoder.ScalingSettings scalingSettings) {
    return scalingSettings.low;
  }

  @Nullable
  @CalledByNative
  static Integer getScalingSettingsHigh(VideoEncoder.ScalingSettings scalingSettings) {
    return scalingSettings.high;
  }

  @Nullable
  @CalledByNative
  static Integer getResolutionBitrateThresholdsCount(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds) {
    return resolutionBitrateThresholds.size();
  }

  @Nullable
  @CalledByNative
  static Integer getResolutionBitrateThresholdsMaxFrameSizePixels(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds, int index) {
    if (index < 0 || index > resolutionBitrateThresholds.size()) {
      return null;
    }
    return resolutionBitrateThresholds.get(index).maxFrameSizePixels;
  }
  @Nullable
  @CalledByNative
  static Integer getResolutionBitrateThresholdsMinStartBitrateBps(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds, int index) {
    if (index < 0 || index > resolutionBitrateThresholds.size()) {
      return null;
    }
    return resolutionBitrateThresholds.get(index).minStartBitrateBps;
  }
  @Nullable
  @CalledByNative
  static Integer getResolutionBitrateThresholdsMinBitrateBps(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds, int index) {
    if (index < 0 || index > resolutionBitrateThresholds.size()) {
      return null;
    }
    return resolutionBitrateThresholds.get(index).minBitrateBps;
  }
  @Nullable
  @CalledByNative
  static Integer getResolutionBitrateThresholdsMaxBitrateBps(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds, int index) {
    if (index < 0 || index > resolutionBitrateThresholds.size()) {
      return null;
    }
    return resolutionBitrateThresholds.get(index).maxBitrateBps;
  }

  @CalledByNative
  static VideoEncoder.Callback createEncoderCallback(final long nativeEncoder) {
    return (EncodedImage frame,
               VideoEncoder.CodecSpecificInfo info) -> nativeOnEncodedFrame(nativeEncoder, frame);
  }

  private static native void nativeOnEncodedFrame(
      long nativeVideoEncoderWrapper, EncodedImage frame);
}
