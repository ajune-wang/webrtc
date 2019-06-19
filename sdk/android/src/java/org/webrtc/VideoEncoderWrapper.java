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
import java.util.ArrayList;
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

  @CalledByNative
  static int[] getResolutionBitrateThresholdsFrameSizePixels(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds) {
    int frameSizePixels[] = new int[resolutionBitrateThresholds.size()];
    for (int i = 0; i < resolutionBitrateThresholds.size(); ++i) {
      frameSizePixels[i] = resolutionBitrateThresholds.get(i).frameSizePixels;
    }
    return frameSizePixels;
  }

  @CalledByNative
  static int[] getResolutionBitrateThresholdsMinStartBitrateBps(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds) {
    int minStartBitrateBps[] = new int[resolutionBitrateThresholds.size()];
    for (int i = 0; i < resolutionBitrateThresholds.size(); ++i) {
      minStartBitrateBps[i] = resolutionBitrateThresholds.get(i).minStartBitrateBps;
    }
    return minStartBitrateBps;
  }

  @CalledByNative
  static int[] getResolutionBitrateThresholdsMinBitrateBps(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds) {
    int minBitrateBps[] = new int[resolutionBitrateThresholds.size()];
    for (int i = 0; i < resolutionBitrateThresholds.size(); ++i) {
      minBitrateBps[i] = resolutionBitrateThresholds.get(i).minBitrateBps;
    }
    return minBitrateBps;
  }

  @CalledByNative
  static int[] getResolutionBitrateThresholdsMaxBitrateBps(
      List<VideoEncoder.ResolutionBitrateThresholds> resolutionBitrateThresholds) {
    int maxBitrateBps[] = new int[resolutionBitrateThresholds.size()];
    for (int i = 0; i < resolutionBitrateThresholds.size(); ++i) {
      maxBitrateBps[i] = resolutionBitrateThresholds.get(i).maxBitrateBps;
    }
    return maxBitrateBps;
  }

  @CalledByNative
  static VideoEncoder.Callback createEncoderCallback(final long nativeEncoder) {
    return (EncodedImage frame,
               VideoEncoder.CodecSpecificInfo info) -> nativeOnEncodedFrame(nativeEncoder, frame);
  }

  private static native void nativeOnEncodedFrame(
      long nativeVideoEncoderWrapper, EncodedImage frame);
}
