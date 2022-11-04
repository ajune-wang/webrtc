/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/** Creates a native {@code webrtc::VideoDecoderFactory} with the builtin video encoders. */
public class BuiltinVideoEncoderFactoryFactory {
  /**
   * Wraps a native webrtc::VideoEncoderFactory.
   */
  public abstract class WrappedNativeVideoEncoderFactory implements VideoEncoderFactory {
    @Override public abstract long createNativeVideoEncoderFactory();

    @Override
    public final VideoDecoder createEncoder(VideoCodecInfo info) {
      throw new UnsupportedOperationException("Not implemented.");
    }

    @Override
    public final VideoCodecInfo[] getSupportedCodecs() {
      throw new UnsupportedOperationException("Not implemented.");
    }
  }

  public static VideoEncoderFactory CreateBuiltinVideoEncoderFactory() {
    return new WrapperVideoEncoderFactory() {
      @Override
      public long createNativeVideoEncoderFactory() {
        return nativeCreateBuiltinVideoEncoderFactory();
      }
    };
  }

  private static native long nativeCreateBuiltinVideoEncoderFactory();
}
