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
import org.chromium.base.annotations.NativeMethods;

import androidx.annotation.Nullable;
import java.util.Arrays;
import java.util.List;

public class SoftwareVideoDecoderFactory implements VideoDecoderFactory {
  private static final String TAG = "SoftwareVideoDecoderFactory";

  private final long nativeFactory;

  public SoftwareVideoDecoderFactory() {
    this.nativeFactory = SoftwareVideoDecoderFactoryJni.get().createFactory();
  }

  @Nullable
  @Override
  public VideoDecoder createDecoder(VideoCodecInfo info) {
    long nativeDecoder = SoftwareVideoDecoderFactoryJni.get().createDecoder(nativeFactory, info);
    if (nativeDecoder == 0) {
      Logging.w(TAG, "Trying to create decoder for unsupported format. " + info);
      return null;
    }

    return new WrappedNativeVideoDecoder() {
      @Override
      public long createNativeVideoDecoder() {
        return nativeDecoder;
      }
    };
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return SoftwareVideoDecoderFactoryJni.get()
        .getSupportedCodecs(nativeFactory)
        .toArray(new VideoCodecInfo[0]);
  }

  @NativeMethods
  interface Natives {
    long createFactory();
    long createDecoder(long factory, VideoCodecInfo videoCodecInfo);
    List<VideoCodecInfo> getSupportedCodecs(long factory);
  }
}
