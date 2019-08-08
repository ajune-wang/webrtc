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

import static org.webrtc.MediaCodecUtils.HARDWARE_IMPLEMENTATION_PREDICATE;

import android.media.MediaCodecInfo;
import android.support.annotation.Nullable;

/** Factory for android hardware video encoders. */
@SuppressWarnings("deprecation") // API 16 requires the use of deprecated methods.
public class HardwareVideoEncoderFactory extends MediaCodecVideoEncoderFactory {
  /**
   * Creates a HardwareVideoEncoderFactory.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *     this disables texture support.
   * @param enableH264HighProfile true if H264 High Profile enabled.
   */
  public HardwareVideoEncoderFactory(
      @Nullable EglBase14.Context sharedContext, boolean enableH264HighProfile) {
    this(sharedContext, enableH264HighProfile, /* codecAllowedPredicate= */ null);
  }

  /**
   * Creates a HardwareVideoEncoderFactory.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *     this disables texture support.
   * @param enableH264HighProfile true if H264 High Profile enabled.
   * @param codecAllowedPredicate optional predicate to filter codecs. All codecs are allowed when
   *     predicate is not provided.
   */
  public HardwareVideoEncoderFactory(@Nullable EglBase14.Context sharedContext,
      boolean enableH264HighProfile, @Nullable Predicate<MediaCodecInfo> codecAllowedPredicate) {
    super(sharedContext, enableH264HighProfile,
        codecAllowedPredicate == null
            ? HARDWARE_IMPLEMENTATION_PREDICATE
            : codecAllowedPredicate.and(HARDWARE_IMPLEMENTATION_PREDICATE));
  }

  /**
   * Creates a HardwareVideoEncoderFactory that supports surface texture encoding.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *     this disables texture support.
   * @param enableIntelVp8Encoder Deprecated and ignored, please use predicate.
   * @param enableH264HighProfile true if H264 High Profile enabled.
   */
  @Deprecated
  public HardwareVideoEncoderFactory(
      EglBase.Context sharedContext, boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this((EglBase14.Context) sharedContext, enableH264HighProfile,
        /* codecAllowedPredicate= */ null);
  }

  /**
   * Creates a HardwareVideoEncoderFactory that supports surface texture encoding.
   *
   * @param sharedContext The textures generated will be accessible from this context. May be null,
   *     this disables texture support.
   * @param enableIntelVp8Encoder Deprecated and ignore, please use predicate.
   * @param enableH264HighProfile true if H264 High Profile enabled.
   * @param codecAllowedPredicate optional predicate to filter codecs. All codecs are allowed when
   *     predicate is not provided.
   */
  @Deprecated
  public HardwareVideoEncoderFactory(EglBase.Context sharedContext, boolean enableIntelVp8Encoder,
      boolean enableH264HighProfile, @Nullable Predicate<MediaCodecInfo> codecAllowedPredicate) {
    this((EglBase14.Context) sharedContext, enableH264HighProfile,
        /* codecAllowedPredicate= */ codecAllowedPredicate);
  }

  @Deprecated
  public HardwareVideoEncoderFactory(boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this(/* sharedContext= */ null, enableH264HighProfile, /* codecAllowedPredicate= */ null);
  }
}
