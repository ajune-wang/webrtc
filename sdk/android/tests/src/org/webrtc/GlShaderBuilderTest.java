/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;

import java.util.Arrays;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

/**
 * Tests for GlShaderBuilder.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlShaderBuilderTest {
  private static final String GENERIC_FRAGMENT_SHADER = "void main() {\n"
      + "  gl_FragColor = sample(tc);\n"
      + "}\n";

  private static final GlShaderBuilder shaderBuilder = new GlShaderBuilder(GENERIC_FRAGMENT_SHADER);

  @Test
  public void testVertexShader() {
    final String expectedVertexShader = "varying vec2 tc;\n"
        + "attribute vec4 in_vc;\n"
        + "attribute vec4 in_tc;\n"
        + "uniform mat4 tex_mat;\n"
        + "void main() {\n"
        + "  gl_Position = in_vc;\n"
        + "  tc = (tex_mat * in_tc).xy;\n"
        + "}\n";
    assertEquals(expectedVertexShader, shaderBuilder.createVertexShaderString());
  }

  @Test
  public void testFragmentOesShader() {
    final String expectedFragmentShader = "#extension GL_OES_EGL_image_external : require\n"
        + "precision mediump float;\n"
        + "varying vec2 tc;\n"
        + "uniform samplerExternalOES oes_tex;\n"
        + "void main() {\n"
        + "  gl_FragColor = texture2D(oes_tex, tc);\n"
        + "}\n";
    assertEquals(expectedFragmentShader,
        shaderBuilder.createFragmentShaderString(GlShaderBuilder.ShaderType.OES));
  }

  @Test
  public void testFragmentRgbShader() {
    final String expectedFragmentShader = "precision mediump float;\n"
        + "varying vec2 tc;\n"
        + "uniform sampler2D rgb_tex;\n"
        + "void main() {\n"
        + "  gl_FragColor = texture2D(rgb_tex, tc);\n"
        + "}\n";
    assertEquals(expectedFragmentShader,
        shaderBuilder.createFragmentShaderString(GlShaderBuilder.ShaderType.RGB));
  }

  @Test
  public void testFragmentYuvShader() {
    final String expectedFragmentShader = "precision mediump float;\n"
        + "varying vec2 tc;\n"
        + "uniform sampler2D y_tex;\n"
        + "uniform sampler2D u_tex;\n"
        + "uniform sampler2D v_tex;\n"
        + "vec4 sample(vec2 p) {\n"
        + "  float y = texture2D(y_tex, p).r;\n"
        + "  float u = texture2D(u_tex, p).r - 0.5;\n"
        + "  float v = texture2D(v_tex, p).r - 0.5;\n"
        + "  return vec4(y + 1.403 * v, y - 0.344 * u - 0.714 * v, y + 1.77 * u, 1);\n"
        + "}\n"
        + "void main() {\n"
        + "  gl_FragColor = sample(tc);\n"
        + "}\n";
    assertEquals(expectedFragmentShader,
        shaderBuilder.createFragmentShaderString(GlShaderBuilder.ShaderType.YUV));
  }
}
