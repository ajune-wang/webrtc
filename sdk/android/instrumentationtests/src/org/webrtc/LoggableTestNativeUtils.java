/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;
import org.chromium.base.annotations.NativeMethods;

// TODO(landrey): The native methods are moved into a separate class because
// of jni_generator bug. See http://go/crb/webrtc/14839#c15 for more details.
// We may want to merge them back after the bug is eliminated.

/** Native methods for LoggableTest.java. */
public class LoggableTestNativeUtils {
  public static void nativeLogInfoTestMessage(String message) {
    LoggableTestNativeUtilsJni.get().logInfoTestMessage(message);
  }

  @NativeMethods
  interface Natives {
    void logInfoTestMessage(String message);
  }
}
