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
import org.webrtc.VideoFrame;

// TODO(landrey): The native methods are moved into a separate class because
// of jni_generator bug. See http://go/crb/webrtc/14839#c15 for more details.
// We may want to merge them back after the bug is eliminated.

/** Native methods for LoggableTest.java. */
public class VideoFrameBufferTestNativeUtils {
  @VideoFrameBufferType
  public static int nativeGetBufferType(VideoFrame.Buffer buffer) {
    return VideoFrameBufferTestNativeUtilsJni.get().getBufferType(buffer);
  }

  public static VideoFrame.Buffer nativeGetNativeI420Buffer(VideoFrame.I420Buffer i420Buffer) {
    return VideoFrameBufferTestNativeUtilsJni.get().getNativeI420Buffer(i420Buffer);
  }

  @NativeMethods
  interface Natives {
    // @VideoFrameBufferType
    int getBufferType(VideoFrame.Buffer buffer);
    /** Returns the copy of I420Buffer using WrappedNativeI420Buffer. */
    VideoFrame.Buffer getNativeI420Buffer(VideoFrame.I420Buffer i420Buffer);
  }
}
