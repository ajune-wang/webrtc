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

import androidx.annotation.Nullable;

/** Factory for creating VideoDecoders. */
public interface VideoDecoderFactory {
  /**
   * Creates a VideoDecoder for the given codec. Supports the same codecs supported by
   * VideoEncoderFactory.
   * `webrtcEnvRef` is a c++ type `const webrtc::Environment*`, i.e. it can be
   * passed to the native api function that expects webrtcEnvRef, but
   * may become invalid after the function returns, i.e., can't be stored.
   * For the transition period while createDecoder variant without webrtcEnvRef
   * exists, it may be 0, but shouldn't once all code migrates to
   * `createDecoder` that has the `webrtcEnvRef` parameter.
   */

  @Nullable
  @Deprecated
  VideoDecoder createDecoder(VideoCodecInfo info);

  @Nullable
  @CalledByNative
  default VideoDecoder createDecoder(long webrtcEnvRef, VideoCodecInfo info) {
    // TODO: bugs.webrtc.org/15791 - Remove default implementation when implemented
    // by all derived classes
    return createDecoder(info);
  }

  /**
   * Enumerates the list of supported video codecs.
   */
  @CalledByNative
  default VideoCodecInfo[] getSupportedCodecs() {
    return new VideoCodecInfo[0];
  }
}
