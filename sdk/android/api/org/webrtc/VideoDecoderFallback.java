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

/**
 * A combined video decoder that falls back on a secondary decoder if the primary decoder fails.
 */
public class VideoDecoderFallback extends WrappedNativeVideoDecoder {
  private final long nativeDecoder;

  @Deprecated
  public VideoDecoderFallback(VideoDecoder fallback, VideoDecoder primary) {
    this.nativeDecoder = nativeCreateDecoder(fallback, primary);
  }

  public VideoDecoderFallback(long webrtcEnvRef, VideoDecoder fallback, VideoDecoder primary) {
    // TODO: bugs.webrtc.org/10335 - In VideoDecoderSoftwareFallbackWrapper
    // use field trial from the propagated webrtcEnvRef instead of the global
    // field trial when usage of the deprecated VideoDecoderFallback constructor
    // is removed.
    this.nativeDecoder = nativeCreateDecoder(fallback, primary);
  }

  @Override
  public long createNativeVideoDecoder() {
    return nativeDecoder;
  }

  private static native long nativeCreateDecoder(VideoDecoder fallback, VideoDecoder primary);
}
