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

/** Creates a native {@code webrtc::VideoDecoderFactory} with the builtin video decoders. */
public class BuiltinVideoDecoderFactoryFactory {
  /**
   * Wraps a native webrtc::VideoDecoderFactory.
   */
  public abstract class WrappedNativeVideoDecoderFactory implements VideoDecoderFactory {
    @Override public abstract long createNativeVideoDecoderFactory();

    @Override
    public final VideoDecoder createDecoder(VideoCodecInfo info) {
      throw new UnsupportedOperationException("Not implemented.");
    }

    @Override
    public final VideoCodecInfo[] getSupportedCodecs() {
      throw new UnsupportedOperationException("Not implemented.");
    }
  }

  public static VideoDecoderFactory CreateBuiltinVideoDecoderFactory() {
    return new WrapperVideoDecoderFactory() {
      @Override
      public long createNativeVideoDecoderFactory() {
        return nativeCreateBuiltinVideoDecoderFactory();
      }
    };
  }

  private static native long nativeCreateBuiltinVideoDecoderFactory();
}
