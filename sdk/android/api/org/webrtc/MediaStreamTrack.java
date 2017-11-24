/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/** Java wrapper for a C++ MediaStreamTrackInterface. */
public class MediaStreamTrack {
  /** Tracks MediaStreamTrackInterface.TrackState */
  public enum State { LIVE, ENDED }

  public enum MediaType {
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_VIDEO;

    @CalledByNative("MediaType")
    static MediaType getAudioType() {
      return MEDIA_TYPE_AUDIO;
    }

    @CalledByNative("MediaType")
    static MediaType getVideoType() {
      return MEDIA_TYPE_VIDEO;
    }
  }

  final long nativeTrack;

  public MediaStreamTrack(long nativeTrack) {
    this.nativeTrack = nativeTrack;
  }

  public String id() {
    return getNativeId(nativeTrack);
  }

  public String kind() {
    return getNativeKind(nativeTrack);
  }

  public boolean enabled() {
    return getNativeEnabled(nativeTrack);
  }

  public boolean setEnabled(boolean enable) {
    return setNativeEnabled(nativeTrack, enable);
  }

  public State state() {
    return getNativeState(nativeTrack);
  }

  public void dispose() {
    JniCommon.nativeReleaseRef(nativeTrack);
  }

  private static native String getNativeId(long nativeTrack);

  private static native String getNativeKind(long nativeTrack);

  private static native boolean getNativeEnabled(long nativeTrack);

  private static native boolean setNativeEnabled(long nativeTrack, boolean enabled);

  private static native State getNativeState(long nativeTrack);
}
