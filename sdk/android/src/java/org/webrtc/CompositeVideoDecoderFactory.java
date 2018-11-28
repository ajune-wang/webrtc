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

/** Helper class that combines primary and fallback decoders. */
class CompositeVideoDecoderFactory implements VideoDecoderFactory {
  private final VideoDecoderFactory primaryVideoDecoderFactory;
  private final VideoDecoderFactory fallbackVideoDecoderFactory;

  /** Create decoder factory using primary and fallback decoder factory. */
  CompositeVideoDecoderFactory(VideoDecoderFactory primaryVideoDecoderFactory,
      VideoDecoderFactory fallbackVideoDecoderFactory) {
    this.primaryVideoDecoderFactory = primaryVideoDecoderFactory;
    this.fallbackVideoDecoderFactory = fallbackVideoDecoderFactory;
  }

  @Override
  public @Nullable VideoDecoder createDecoder(VideoCodecInfo info) {
    final VideoDecoder primaryDecoder = primaryVideoDecoderFactory.createDecoder(info);
    final VideoDecoder fallbackDecoder = fallbackVideoDecoderFactory.createDecoder(info);

    if (primaryDecoder != null && fallbackDecoder != null) {
      // Both primary and fallback supported, wrap it in a software fallback
      return new VideoDecoderFallback(
          /* fallback= */ fallbackDecoder, /* primary= */ primaryDecoder);
    }
    return primaryDecoder != null ? primaryDecoder : fallbackDecoder;
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    LinkedHashSet<VideoCodecInfo> supportedCodecInfos = new LinkedHashSet<VideoCodecInfo>();

    supportedCodecInfos.addAll(Arrays.asList(primaryVideoDecoderFactory.getSupportedCodecs()));
    supportedCodecInfos.addAll(Arrays.asList(fallbackVideoDecoderFactory.getSupportedCodecs()));

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }
}
