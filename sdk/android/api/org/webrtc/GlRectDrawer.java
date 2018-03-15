/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * Helper class to draw an opaque quad on the target viewport location. Rotation, mirror, and
 * cropping is specified using a 4x4 texture coordinate transform matrix. The frame input can either
 * be an OES texture or YUV textures in I420 format. The GL state must be preserved between draw
 * calls, this is intentional to maximize performance. The function release() must be called
 * manually to free the resources held by this object.
 */
public class GlRectDrawer extends GlGenericDrawer {
  private static final String FRAGMENT_SHADER = "void main() {\n"
      + "  gl_FragColor = sample(tc);\n"
      + "}\n";

  private static final GlShaderBuilder shaderBuilder = new GlShaderBuilder(FRAGMENT_SHADER);

  private static final GlShaderBuilder.ShaderCallbacks shaderCallbacks =
      new GlShaderBuilder.ShaderCallbacks() {
        @Override
        public void onNewShader(GlShaderBuilder.ShaderType shaderType, GlShader shader) {}

        @Override
        public void onPrepareShader(GlShaderBuilder.ShaderType shaderType, GlShader shader,
            float[] texMatrix, int frameWidth, int frameHeight, int viewportWidth,
            int viewportHeight) {}
      };

  public static RendererCommon.GlDrawer create() {
    return shaderBuilder.createGlDrawer(shaderCallbacks);
  }

  /** Use GlRectDrawer.create() instead. */
  @Deprecated
  public GlRectDrawer() {
    super(shaderBuilder, shaderCallbacks);
  }
}
