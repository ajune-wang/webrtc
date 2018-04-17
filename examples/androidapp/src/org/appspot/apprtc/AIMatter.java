/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.content.res.AssetManager;
import android.util.Log;

public class AIMatter {
  static {
    System.loadLibrary("AppRTCMobile_jni");
  }

  private native int nativeTestJniFunc(int i);
  private native int nativeFabbyVideoSegment(long info_ptr, int texture_id, int width, int height,
      int camera_angle, int camera_facing, byte[] mask);
  // mask_texture stores mask_texture_id, width, height
  private native int nativeFabbyVideoSegment2(long info_ptr, int texture_id, int width, int height,
      int camera_angle, int camera_facing, int[] mask_texture);
  private native long nativeInitFabbyVideoSegmenter(AssetManager manager, String path);
  private native void nativeDestroyFabbyVideoSegmenter(long segmenter_handle);

  private long segmenter_handle;

  public int TestJniFunc(int i) {
    return nativeTestJniFunc(i);
  }

  public AIMatter(AssetManager manager, String path) {
    segmenter_handle = nativeInitFabbyVideoSegmenter(manager, path);
    Log.e("AIMatter", "Handle: " + segmenter_handle);
  }

  public void Destroy() {
    nativeDestroyFabbyVideoSegmenter(segmenter_handle);
    segmenter_handle = 0;
  }

  public int Segment(int texture_id, int width, int height, int camera_angle, int camera_facing,
      int[] mask_texture) {
    if (segmenter_handle == 0)
      return 0;

    return nativeFabbyVideoSegment2(
        segmenter_handle, texture_id, width, height, camera_angle, camera_facing, mask_texture);
  }
}
