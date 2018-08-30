/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * FrameDecryptors are extremely performance sensitive as they must process all
 * incoming video and audio frames. Due to this reason they should almost
 * always be backed by a native implementation.
 * Note:
 * Not ready for production use.
 */
public interface FrameDecryptor {
  /**
   * @return A FrameDecryptorInterface pointer.
   */
  @CalledByNative long getNativeFrameDecryptor();
}
