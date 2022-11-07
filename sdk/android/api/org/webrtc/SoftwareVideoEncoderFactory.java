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

public class SoftwareVideoEncoderFactory implements VideoEncoderFactory {
  private static final String TAG = "SoftwareVideoEncoderFactory";

  private final long nativeFactory;

  public SoftwareVideoEncoderFactory() {
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
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    if (this.nativeFactory == 0) {
      Logging.e(TAG, "Failed to create video encoder. Native encoder factory is not available.");
      return null;
    }
    return new WrappedNativeVideoEncoder() {
      @Override
      public long createNativeVideoEncoder() {
        return nativeCreateEncoder(nativeFactory, info);
      }

      @Override
      public boolean isHardwareEncoder() {
        return false;
      }
    };
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    if (this.nativeFactory == 0) {
      Logging.e(
          TAG, "Failed to query supported decoders. Native encoder factory is not available.");
      return new VideoCodecInfo[0];
    }
    return nativeGetSupportedCodecs(nativeFactory).toArray(new VideoCodecInfo[0]);
  }

  private static native long nativeCreateFactory();

  private static native long nativeCreateEncoder(long factory, VideoCodecInfo videoCodecInfo);

  private static native List<VideoCodecInfo> nativeGetSupportedCodecs(long factory);
}
