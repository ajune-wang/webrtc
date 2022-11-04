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
  public static long createNativeVideoEncoderFactory() {
    return nativeCreateBuiltinVideoEncoderFactory();
  }

  private static native long nativeCreateBuiltinVideoEncoderFactory();
}
