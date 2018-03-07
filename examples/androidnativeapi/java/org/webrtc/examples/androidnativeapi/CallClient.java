/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidnativeapi;

import android.os.Handler;
import android.os.HandlerThread;
import org.webrtc.CalledByNative;
import org.webrtc.NativeClassQualifiedName;
import org.webrtc.VideoSink;

public class CallClient {
  private static final String TAG = "CallClient";

  private final HandlerThread thread;
  private final long nativeClient;
  private final Handler handler;

  public CallClient(HandlerThread thread, long nativeClient) {
    this.thread = thread;
    this.nativeClient = nativeClient;

    handler = new Handler(thread.getLooper());
  }

  public static CallClient create() {
    HandlerThread thread = new HandlerThread(TAG + "Thread");
    thread.start();
    return new CallClient(thread, nativeCreateClient());
  }

  public void call(VideoSink localSink, VideoSink remoteSink) {
    handler.post(() -> { nativeCall(nativeClient, localSink, remoteSink); });
  }

  public void hangup() {
    handler.post(() -> { nativeHangup(nativeClient); });
  }

  public void close() {
    handler.post(() -> {
      nativeDelete(nativeClient);
    });
    thread.quitSafely();
  }

  private static native long nativeCreateClient();
  @NativeClassQualifiedName("webrtc_examples::AndroidCallClient")
  private static native void nativeCall(long nativePtr, VideoSink localSink, VideoSink remoteSink);
  @NativeClassQualifiedName("webrtc_examples::AndroidCallClient")
  private static native void nativeHangup(long nativePtr);
  @NativeClassQualifiedName("webrtc_examples::AndroidCallClient")
  private static native void nativeDelete(long nativePtr);
}
