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

import javax.annotation.Nullable;

/**
 * Factory for android hardware video encoders.
 */
public class HardwareVideoEncoderFactory extends MediaCodecVideoEncoderFactory {
  /**
   * Creates a HardwareVideoEncoderFactory that supports surface texture rendering.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *                      this disables texture support.
   * @param enableH264HighProfile Enables H264 high-profile support. This should only be set to true
   *                              if the decoder used has support for high-profile.
   */
  public HardwareVideoEncoderFactory(
      @Nullable EglBase14.Context sharedContext, boolean enableH264HighProfile) {
    super(sharedContext, new String[] {""}, MediaCodecUtils.SOFTWARE_IMPLEMENTATION_PREFIXES,
        enableH264HighProfile);
  }

  /**
   * Creates a HardwareVideoEncoderFactory that supports surface texture rendering.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *                      this disables texture support.
   * @param enableH264HighProfile Enables H264 high-profile support. This should only be set to true
   *                              if the decoder used has support for high-profile.
   * @param enableIntelVp8Encoder Deprecated and ignored.
   */
  @Deprecated
  public HardwareVideoEncoderFactory(@Nullable EglBase.Context sharedContext,
      boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this(toEglBase14Context(sharedContext), enableH264HighProfile);
  }

  /**
   * Creates a HardwareVideoEncoderFactory that doesn't support surface texture rendering.
   *
   * @param enableH264HighProfile Enables H264 high-profile support. This should only be set to true
   *                              if the decoder used has support for high-profile.
   * @param enableIntelVp8Encoder Deprecated and ignored.
   */
  @Deprecated
  public HardwareVideoEncoderFactory(boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this(null, enableH264HighProfile);
  }

  private static @Nullable EglBase14.Context toEglBase14Context(@Nullable EglBase.Context context) {
    if (context instanceof EglBase14.Context) {
      return (EglBase14.Context) context;
    } else {
      return null;
    }
  }
}
