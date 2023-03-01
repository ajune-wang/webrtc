/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;
import org.webrtc.NativeMethods;

public class LibvpxVp9Decoder extends WrappedNativeVideoDecoder {
  @Override
  public long createNativeVideoDecoder() {
    return LibvpxVp9DecoderJni.get().createDecoder();
  }

  @NativeMethods
  interface Natives {
    long createDecoder();
    boolean isSupported();
  }
}
