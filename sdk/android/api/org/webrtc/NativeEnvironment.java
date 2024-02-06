/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * This class contains the Java wrapper of the webrtc::Environment object.
 */
class NativeEnvironment {
  // owning pointer to the webrtc::Environment.
  private long webrtcEnvironment;

  public NativeEnvironment() {
    webrtcEnvironment = nativeCreateDefaultEnvironment();
  }

  // returns non-owning pointer to the webrtc::Environment
  public long ref() {
    return webrtcEnvironment;
  }

  public void dispose() {
    nativeDeleteEnvironment(webrtcEnvironment);
    webrtcEnvironment = 0;
  }

  private static native long nativeCreateDefaultEnvironment();
  private static native void nativeDeleteEnvironment(long webrtcEnv);
}
