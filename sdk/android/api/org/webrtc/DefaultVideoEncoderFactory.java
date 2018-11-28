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
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;

/** Helper class that combines HW and SW encoders. */
public class DefaultVideoEncoderFactory implements VideoEncoderFactory {
  private final VideoEncoderFactory impl;

  /** Create encoder factory using default hardware encoder factory. */
  public DefaultVideoEncoderFactory(
      EglBase.Context eglContext, boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this(new HardwareVideoEncoderFactory(eglContext, enableIntelVp8Encoder, enableH264HighProfile));
  }

  /** Create encoder factory using explicit hardware encoder factory. */
  DefaultVideoEncoderFactory(VideoEncoderFactory hardwareVideoEncoderFactory) {
    this.impl = hardwareVideoEncoderFactory.withFallbackTo(new SoftwareVideoEncoderFactory());
  }

  @Nullable
  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    return impl.createEncoder(info);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return impl.getSupportedCodecs();
  }
}
