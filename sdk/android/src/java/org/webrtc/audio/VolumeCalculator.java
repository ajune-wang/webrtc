/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import java.nio.ByteBuffer;
import java.nio.ShortBuffer;

/**
 * Private utility class that continously calculates the volume level in captured audio frames.
 * The calculator reports a volume every 10'th frame that is based on the maximum absolute
 * value observed in the frame set.
 */
class VolumeCalculator {
  private static final int FRAME_WINDOW_SIZE = 10;

  private int count;
  private int max;
  private int volume;

  boolean addSample(byte[] data) {
    ShortBuffer sbuf = ByteBuffer.wrap(data).asShortBuffer();
    short[] audioShorts = new short[sbuf.capacity()];
    sbuf.get(audioShorts);

    for (short audioShort : audioShorts) {
      int val = Math.abs(Short.reverseBytes(audioShort));
      max = Math.max(max, val);
    }

    if (count == FRAME_WINDOW_SIZE) {
      calculateVolume();
      reset();
      return true;
    } else {
      count++;
      return false;
    }
  }

  int getVolume() {
    return volume;
  }

  private void calculateVolume() {
    double f = (max / 32767.0);
    volume = (int) (Math.cbrt(f) * 10);
  }

  private void reset() {
    count = 0;
    max = 0;
  }
}
