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
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class SoftwareVideoDecoderFactory implements VideoDecoderFactory {
  private static final String TAG = "SoftwareVideoDecoderFactory";

  private final long nativeFactory;

  public SoftwareVideoDecoderFactory() {
    long nativeFactory = 0;
    try {
      nativeFactory = nativeCreateFactory();
    } catch (UnsatisfiedLinkError e) {
      Logging.e(TAG, "Attempting to create the native factory without the native code", e);
    }
    this.nativeFactory = nativeFactory;
  }

  @Nullable
  @Override
  public VideoDecoder createDecoder(VideoCodecInfo info) {
    if (this.nativeFactory == 0) {
      Logging.e(TAG, "Failed to create video decoder. Native decoder factory is not available.");
      return null;
    }
    return new WrappedNativeVideoDecoder() {
      @Override
      public long createNativeVideoDecoder() {
        return nativeCreateDecoder(nativeFactory, info);
      }
    };
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    if (this.nativeFactory == 0) {
      Logging.e(
          TAG, "Failed to query supported decoders. Native decoder factory is not available.");
      return new VideoCodecInfo[0];
    }
    return nativeGetSupportedCodecs(nativeFactory).toArray(new VideoCodecInfo[0]);
  }

  private static native long nativeCreateFactory();

  private static native long nativeCreateDecoder(long factory, VideoCodecInfo videoCodecInfo);

  private static native List<VideoCodecInfo> nativeGetSupportedCodecs(long factory);
}
}
