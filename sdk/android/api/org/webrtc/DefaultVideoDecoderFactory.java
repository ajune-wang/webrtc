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

/**
 * Helper class that combines HW and SW decoders.
 */
public class DefaultVideoDecoderFactory implements VideoDecoderFactory {
  private final VideoDecoderFactory impl;

  /**
   * Create decoder factory using default hardware decoder factory.
   */
  public DefaultVideoDecoderFactory(@Nullable EglBase.Context eglContext) {
    final VideoDecoderFactory hardwareVideoDecoderFactory =
        new HardwareVideoDecoderFactory(eglContext);
    final VideoDecoderFactory platformSoftwareVideoDecoderFactory =
        new PlatformSoftwareVideoDecoderFactory(eglContext);
    final VideoDecoderFactory softwareVideoDecoderFactory = new SoftwareVideoDecoderFactory();

    this.impl = hardwareVideoDecoderFactory.withFallbackTo(
        softwareVideoDecoderFactory.withFallbackTo(platformSoftwareVideoDecoderFactory));
  }

  /**
   * Create decoder factory using explicit hardware decoder factory.
   */
  DefaultVideoDecoderFactory(VideoDecoderFactory hardwareVideoDecoderFactory) {
    this.impl = hardwareVideoDecoderFactory.withFallbackTo(new SoftwareVideoDecoderFactory());
  }

  @Override
  public @Nullable VideoDecoder createDecoder(VideoCodecInfo codecType) {
    return impl.createDecoder(codecType);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return impl.getSupportedCodecs();
  }
}
