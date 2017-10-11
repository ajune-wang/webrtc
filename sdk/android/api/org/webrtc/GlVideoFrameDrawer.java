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

/**
 * Interface for a drawer that is capable of rendering a video frame using OpenGL.
 */
public interface GlVideoFrameDrawer {
  /**
   * Draws a frame to the current OpenGL viewport. If VideoFrame buffer is a texture buffer, the
   * texture must be accessible from the current GL context. Transformations such as frame rotation
   * are automatically applied. Additional render matrix is preconcatinated to the other
   * transformations.
   *
   * @param frame The frame to be drawn. Any textures must be available in the current GL context.
   * @param additionalRenderMatrix A matrix that will be preconcatinated to the render matrix.
   * @param viewportX X-coordinate of the top-left corner in viewport space.
   * @param viewportY Y-coordinate of the top-left corner in viewport space.
   * @param viewportWidth Width of the area to be drawn to in viewport space.
   * @param viewportHeight Height of the area to be drawn to in viewport space.
   */
  void drawFrame(VideoFrame frame, Matrix additionalRenderMatrix, int viewportX, int viewportY,
      int viewportWidth, int viewportHeight);

  /**
   * Releases the resources held by the drawer. Released resources are automatically reallocated if
   * drawFrame is called after this.
   */
  void dispose();
}
