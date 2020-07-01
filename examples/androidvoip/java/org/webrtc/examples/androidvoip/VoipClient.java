/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidvoip;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import java.util.ArrayList;
import java.util.List;

public class VoipClient {
  private long nativeClient;

  public VoipClient() {
    nativeClient = nativeCreateClient();
  }

  public boolean initialize() {
    return nativeInitialize(nativeClient);
  }

  public List<String> getSupportedEncoders() {
    return nativeGetSupportedEncoders(nativeClient);
  }

  public List<String> getSupportedDecoders() {
    return nativeGetSupportedDecoders(nativeClient);
  }

  public String getLocalIPAddress() {
    return nativeGetLocalIPAddress(nativeClient);
  }

  public void setEncoder(String encoder) {
    nativeSetEncoder(nativeClient, encoder);
  }

  public void setDecoders(List<String> decoders) {
    nativeSetDecoders(nativeClient, decoders);
  }

  public void setLocalAddress(String ipAddress, int portNumber) {
    nativeSetLocalAddress(nativeClient, ipAddress, portNumber);
  }

  public void setRemoteAddress(String ipAddress, int portNumber) {
    nativeSetRemoteAddress(nativeClient, ipAddress, portNumber);
  }

  public boolean startSession() {
    return nativeStartSession(nativeClient);
  }

  public boolean stopSession() {
    return nativeStopSession(nativeClient);
  }

  public boolean startSend() {
    return nativeStartSend(nativeClient);
  }

  public boolean stopSend() {
    return nativeStopSend(nativeClient);
  }

  public boolean startPlayout() {
    return nativeStartPlayout(nativeClient);
  }

  public boolean stopPlayout() {
    return nativeStopPlayout(nativeClient);
  }

  public void close() {
    nativeDelete(nativeClient);
    nativeClient = 0;
  }

  private static native long nativeCreateClient();
  private static native boolean nativeInitialize(long nativeAndroidVoipClient);
  private static native List<String> nativeGetSupportedEncoders(long nativeAndroidVoipClient);
  private static native List<String> nativeGetSupportedDecoders(long nativeAndroidVoipClient);
  private static native String nativeGetLocalIPAddress(long nativeAndroidVoipClient);
  private static native void nativeSetEncoder(long nativeAndroidVoipClient, String encoder);
  private static native void nativeSetDecoders(long nativeAndroidVoipClient, List<String> decoders);
  private static native void nativeSetLocalAddress(
      long nativeAndroidVoipClient, String ipAddress, int portNumber);
  private static native void nativeSetRemoteAddress(
      long nativeAndroidVoipClient, String ipAddress, int portNumber);
  private static native boolean nativeStartSession(long nativeAndroidVoipClient);
  private static native boolean nativeStopSession(long nativeAndroidVoipClient);
  private static native boolean nativeStartSend(long nativeAndroidVoipClient);
  private static native boolean nativeStopSend(long nativeAndroidVoipClient);
  private static native boolean nativeStartPlayout(long nativeAndroidVoipClient);
  private static native boolean nativeStopPlayout(long nativeAndroidVoipClient);
  private static native void nativeDelete(long nativeAndroidVoipClient);
}
