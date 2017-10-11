/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.graphics.Matrix;
import org.webrtc.RendererCommon.GlDrawer;

class GlDrawerToGlVideoFrameDrawerAdapter implements GlVideoFrameDrawer {
  private final VideoFrameDrawer videoFrameDrawer = new VideoFrameDrawer();
  private final GlDrawer drawer;

  public GlDrawerToGlVideoFrameDrawerAdapter(GlDrawer drawer) {
    this.drawer = drawer;
  }

  @Override
  public void drawFrame(VideoFrame frame, Matrix additionalRenderMatrix, int viewportX,
      int viewportY, int viewportWidth, int viewportHeight) {
    videoFrameDrawer.drawFrame(
        frame, drawer, additionalRenderMatrix, viewportX, viewportY, viewportWidth, viewportHeight);
  }

  @Override
  public void dispose() {
    drawer.release();
    videoFrameDrawer.release();
  }
}
