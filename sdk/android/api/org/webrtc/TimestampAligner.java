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
import org.chromium.base.annotations.NativeMethods;

/**
 * The TimestampAligner class helps translating camera timestamps into the same timescale as is
 * used by rtc::TimeNanos(). Some cameras have built in timestamping which is more accurate than
 * reading the system clock, but using a different epoch and unknown clock drift. Frame timestamps
 * in webrtc should use rtc::TimeNanos (system monotonic time), and this class provides a filter
 * which lets us use the rtc::TimeNanos timescale, and at the same time take advantage of higher
 * accuracy of the camera clock. This class is a wrapper on top of rtc::TimestampAligner.
 */
public class TimestampAligner {
  /**
   * Wrapper around rtc::TimeNanos(). This is normally same as System.nanoTime(), but call this
   * function to be safe.
   */
  public static long getRtcTimeNanos() {
    return TimestampAlignerJni.get().rtcTimeNanos();
  }

  private volatile long nativeTimestampAligner = TimestampAlignerJni.get().createTimestampAligner();

  /**
   * Translates camera timestamps to the same timescale as is used by rtc::TimeNanos().
   * `cameraTimeNs` is assumed to be accurate, but with an unknown epoch and clock drift. Returns
   * the translated timestamp.
   */
  public long translateTimestamp(long cameraTimeNs) {
    checkNativeAlignerExists();
    return TimestampAlignerJni.get().translateTimestamp(nativeTimestampAligner, cameraTimeNs);
  }

  /** Dispose native timestamp aligner. */
  public void dispose() {
    checkNativeAlignerExists();
    TimestampAlignerJni.get().releaseTimestampAligner(nativeTimestampAligner);
    nativeTimestampAligner = 0;
  }

  private void checkNativeAlignerExists() {
    if (nativeTimestampAligner == 0) {
      throw new IllegalStateException("TimestampAligner has been disposed.");
    }
  }

  @NativeMethods
  interface Natives {
    long rtcTimeNanos();
    long createTimestampAligner();
    void releaseTimestampAligner(long timestampAligner);
    long translateTimestamp(long timestampAligner, long cameraTimeNs);
  }
}
