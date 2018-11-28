/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import javax.annotation.Nullable;

/** Helper class that combines primary and fallback encoders. */
class CompositeVideoEncoderFactory implements VideoEncoderFactory {
  private final VideoEncoderFactory primaryEncoderFactory;
  private final VideoEncoderFactory fallbackEncoderFactory;

  /** Create encoder factory using primary and fallback encoder factories. */
  CompositeVideoEncoderFactory(
      VideoEncoderFactory primaryEncoderFactory, VideoEncoderFactory fallbackEncoderFactory) {
    this.primaryEncoderFactory = primaryEncoderFactory;
    this.fallbackEncoderFactory = fallbackEncoderFactory;
  }

  @Nullable
  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    final VideoEncoder primaryEncoder = primaryEncoderFactory.createEncoder(info);
    final VideoEncoder fallbackEncoder = fallbackEncoderFactory.createEncoder(info);
    if (primaryEncoder != null && fallbackEncoder != null) {
      // Both primary and fallback encodrs supported, wrap it in a software fallback
      return new VideoEncoderFallback(
          /* fallback= */ fallbackEncoder, /* primary= */ primaryEncoder);
    }
    return primaryEncoder != null ? primaryEncoder : fallbackEncoder;
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    // Use linked hash set to maintain codec-order (preference) of original factories
    LinkedHashSet<VideoCodecInfo> supportedCodecInfos = new LinkedHashSet<VideoCodecInfo>();

    supportedCodecInfos.addAll(Arrays.asList(primaryEncoderFactory.getSupportedCodecs()));
    supportedCodecInfos.addAll(Arrays.asList(fallbackEncoderFactory.getSupportedCodecs()));

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }
}
