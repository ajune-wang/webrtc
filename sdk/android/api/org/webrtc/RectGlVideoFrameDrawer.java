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

import android.graphics.Matrix;
import org.webrtc.GenericGlVideoFrameDrawer.FrameFormat;
import org.webrtc.GenericGlVideoFrameDrawer.FrameInformation;

/** Basic video frame drawer that just renders the frame. */
public class RectGlVideoFrameDrawer
    implements GlVideoFrameDrawer, GenericGlVideoFrameDrawer.ShaderDefinition {
  // clang-format off
  // Simple vertex shader, used by all formats.
  private static final String VERTEX_SHADER_STRING =
        "varying vec2 interp_tc;\n"
      + "attribute vec4 in_pos;\n"
      + "attribute vec4 in_tc;\n"
      + "\n"
      + "uniform mat4 tex_matrix;\n"
      + "\n"
      + "void main() {\n"
      + "    gl_Position = in_pos;\n"
      + "    interp_tc = (tex_matrix * in_tc).xy;\n"
      + "}\n";

  private static final String YUV_FRAGMENT_SHADER_STRING =
        "precision mediump float;\n"
      + "varying vec2 interp_tc;\n"
      + "\n"
      + "uniform sampler2D y_tex;\n"
      + "uniform sampler2D u_tex;\n"
      + "uniform sampler2D v_tex;\n"
      + "\n"
      + "void main() {\n"
      // CSC according to http://www.fourcc.org/fccyvrgb.php
      + "  float y = texture2D(y_tex, interp_tc).r;\n"
      + "  float u = texture2D(u_tex, interp_tc).r - 0.5;\n"
      + "  float v = texture2D(v_tex, interp_tc).r - 0.5;\n"
      + "  gl_FragColor = vec4(y + 1.403 * v, "
      + "                      y - 0.344 * u - 0.714 * v, "
      + "                      y + 1.77 * u, 1);\n"
      + "}\n";

  private static final String RGB_FRAGMENT_SHADER_STRING =
        "precision mediump float;\n"
      + "varying vec2 interp_tc;\n"
      + "\n"
      + "uniform sampler2D tex;\n"
      + "\n"
      + "void main() {\n"
      + "  gl_FragColor = texture2D(tex, interp_tc);\n"
      + "}\n";

  private static final String OES_FRAGMENT_SHADER_STRING =
        "#extension GL_OES_EGL_image_external : require\n"
      + "precision mediump float;\n"
      + "varying vec2 interp_tc;\n"
      + "\n"
      + "uniform samplerExternalOES tex;\n"
      + "\n"
      + "void main() {\n"
      + "  gl_FragColor = texture2D(tex, interp_tc);\n"
      + "}\n";
  // clang-format on

  private final GenericGlVideoFrameDrawer drawerDelegate = new GenericGlVideoFrameDrawer(this);

  @Override
  public GlShader getShader(FrameFormat frameFormat) {
    final String fragmentShader;
    if (frameFormat.isYuv()) {
      fragmentShader = YUV_FRAGMENT_SHADER_STRING;
    } else {
      switch (frameFormat.getTextureType()) {
        case RGB:
          fragmentShader = RGB_FRAGMENT_SHADER_STRING;
          break;
        case OES:
          fragmentShader = OES_FRAGMENT_SHADER_STRING;
          break;
        default:
          throw new RuntimeException("Unsupported frame format: " + frameFormat);
      }
    }

    return new GlShader(VERTEX_SHADER_STRING, fragmentShader);
  }

  @Override
  public void prepareShader(FrameFormat frameFormat, GlShader shader) {}

  @Override
  public void updateShader(FrameFormat frameFormat, GlShader shader, FrameInformation frameInfo) {}

  @Override
  public void drawFrame(VideoFrame frame, Matrix additionalRenderMatrix, int viewportX,
      int viewportY, int viewportWidth, int viewportHeight) {
    drawerDelegate.drawFrame(
        frame, additionalRenderMatrix, viewportX, viewportY, viewportWidth, viewportHeight);
  }

  @Override
  public void dispose() {
    drawerDelegate.dispose();
  }
}
