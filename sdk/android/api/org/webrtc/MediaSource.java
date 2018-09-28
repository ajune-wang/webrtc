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

/** Java wrapper for a C++ MediaSourceInterface. */
public class MediaSource {
  /** Tracks MediaSourceInterface.SourceState */
  public enum State {
    INITIALIZING,
    LIVE,
    ENDED,
    MUTED;

    @CalledByNative("State")
    static State fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  private long nativeSource; // Package-protected for PeerConnectionFactory.

  public MediaSource(long nativeSource) {
    this.nativeSource = nativeSource;
  }

  public State state() {
    if (nativeSource == 0) {
      throw new IllegalStateException("MediaSource has been disposed.");
    }
    return nativeGetState(nativeSource);
  }

  public void dispose() {
    if (nativeSource == 0) {
      throw new IllegalStateException("MediaSource has been disposed.");
    }
    JniCommon.nativeReleaseRef(nativeSource);
    nativeSource = 0;
  }

  protected long getNativeMediaSource() {
    if (nativeSource == 0) {
      throw new IllegalStateException("MediaSource has been disposed.");
    }
    return nativeSource;
  }

  private static native State nativeGetState(long pointer);
}
