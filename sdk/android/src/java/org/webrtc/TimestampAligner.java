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
 * The TimestampAligner class helps translating camera timestamps into the same timescale as is
 * used by rtc::TimeNanos(). Some cameras have built in timestamping which is more accurate than
 * reading the system clock, but using a different epoch and unknown clock drift. Frame timestamps
 * in webrtc should use rtc::TimeNanos (system monotonic time), and this class provides a filter
 * which lets us use the rtc::TimeNanos timescale, and at the same time take advantage of higher
 * accuracy of the camera clock. This class is a wrapper on top of rtc::TimestampAligner.
 */
class TimestampAligner {
  private volatile long nativeTimestampAligner = nativeCreateTimestampAligner();

  /**
   * Translates camera timestamps to the same timescale as is used by rtc::TimeNanos().
   * |cameraTimeNs| is assumed to be accurate, but with an unknown epoch and clock drift. Returns
   * the translated timestamp.
   */
  public long translateTimestamp(long cameraTimeNs) {
    return nativeTranslateTimestamp(nativeTimestampAligner, cameraTimeNs);
  }

  /** Release native timestamp aligner. */
  public void release() {
    nativeReleaseTimestampAligner(nativeTimestampAligner);
    nativeTimestampAligner = 0;
  }

  private static native long nativeCreateTimestampAligner();
  private static native void nativeReleaseTimestampAligner(long timestampAligner);
  private static native long nativeTranslateTimestamp(long timestampAligner, long cameraTimeNs);
}
