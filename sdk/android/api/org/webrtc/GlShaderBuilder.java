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

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import java.nio.FloatBuffer;

/**
 * Helper class to build an instance of RendererCommon.GlDrawer that can accept multiple input
 * sources (OES, RGB, or YUV) using a generic fragment shader as input. The generic fragment shader
 * should sample pixel values from the function "sample" that will be provided by this class and
 * provides an abstraction for the input source type (OES, RGB, or YUV). The texture coordinate
 * variable name will be "tc" and the texture matrix in the vertex shader will be "tex_mat". The
 * simplest possible generic shader that just draws pixel from the frame unmodified looks like:
 * void main() {
 *   gl_FragColor = sample(tc);
 * }
 * This class covers the cases for most simple shaders and generates the necessary boiler plate.
 * Advanced shaders can always implement RendererCommon.GlDrawer directly.
 */
public class GlShaderBuilder {
  /**
   * The different shader types representing different input sources. YUV here represents three
   * separate Y, U, V textures.
   */
  public enum ShaderType { OES, RGB, YUV }

  /** Precision qualifier for the shader floats. */
  public enum PrecisionQualifier { LOW, MEDIUM, HIGH }

  /**
   * The shader callbacks is used to customize behavior for a GlDrawer. It provides a hook to set
   * uniform variables in the shader before a frame is drawn.
   */
  public static interface ShaderCallbacks {
    /**
     * This callback is called when a new shader has been compiled and created. It will be called
     * for the first frame as well as when the shader type changes.
     */
    void onNewShader(ShaderType shaderType, GlShader shader);

    /**
     * This callback is called before rendering a frame.
     */
    void onPrepareShader(ShaderType shaderType, GlShader shader, float[] texMatrix, int frameWidth,
        int frameHeight, int viewportWidth, int viewportHeight);
  }

  public static final String INPUT_VERTEX_COORDINATE_NAME = "in_vc";
  public static final String INPUT_TEXTURE_COORDINATE_NAME = "in_tc";
  public static final String TEXTURE_COORDINATE_NAME = "tc";
  public static final String TEXTURE_MATRIX_NAME = "tex_mat";
  public static final String SAMPLE_FUNCTION_NAME = "sample";

  // Vertex coordinates in Normalized Device Coordinates, i.e. (-1, -1) is bottom-left and (1, 1)
  // is top-right.
  private static final FloatBuffer FULL_RECTANGLE_BUFFER = GlUtil.createFloatBuffer(new float[] {
      -1.0f, -1.0f, // Bottom left.
      1.0f, -1.0f, // Bottom right.
      -1.0f, 1.0f, // Top left.
      1.0f, 1.0f, // Top right.
  });

  // Texture coordinates - (0, 0) is bottom-left and (1, 1) is top-right.
  private static final FloatBuffer FULL_RECTANGLE_TEXTURE_BUFFER =
      GlUtil.createFloatBuffer(new float[] {
          0.0f, 0.0f, // Bottom left.
          1.0f, 0.0f, // Bottom right.
          0.0f, 1.0f, // Top left.
          1.0f, 1.0f, // Top right.
      });

  private final String genericFragmentSource;
  private String vertexShader;
  private PrecisionQualifier precisionQualifier = PrecisionQualifier.MEDIUM;

  public GlShaderBuilder(String genericFragmentSource) {
    this.genericFragmentSource = genericFragmentSource;
  }

  public GlShaderBuilder setVertexShader(String vertexShader) {
    this.vertexShader = vertexShader;
    return this;
  }

  public GlShaderBuilder setDefaultPrecision(PrecisionQualifier precisionQualifier) {
    this.precisionQualifier = precisionQualifier;
    return this;
  }

  public RendererCommon.GlDrawer createGlDrawer(ShaderCallbacks shaderCallbacks) {
    return new GlGenericDrawer(this, shaderCallbacks);
  }

  private static String precisionQualifierString(PrecisionQualifier precisionQualifier) {
    switch (precisionQualifier) {
      case LOW:
        return "lowp";
      case MEDIUM:
        return "mediump";
      case HIGH:
        return "highp";
      default:
        throw new IllegalArgumentException();
    }
  }

  // Note: Only valid for RGB and OES, not YUV since they have three textures.
  private static String textureVariableName(ShaderType shaderType) {
    switch (shaderType) {
      case RGB:
        return "rgb_tex";
      case OES:
        return "oes_tex";
      default:
        throw new IllegalArgumentException();
    }
  }

  private static String samplerName(ShaderType shaderType) {
    return shaderType == ShaderType.OES ? "samplerExternalOES" : "sampler2D";
  }

  protected String createVertexShaderString() {
    if (vertexShader != null) {
      return vertexShader;
    }
    final StringBuilder stringBuilder = new StringBuilder();
    stringBuilder.append("varying vec2 " + TEXTURE_COORDINATE_NAME + ";\n");
    stringBuilder.append("attribute vec4 " + INPUT_VERTEX_COORDINATE_NAME + ";\n");
    stringBuilder.append("attribute vec4 " + INPUT_TEXTURE_COORDINATE_NAME + ";\n");
    stringBuilder.append("uniform mat4 " + TEXTURE_MATRIX_NAME + ";\n");
    stringBuilder.append("void main() {\n");
    stringBuilder.append("  gl_Position = " + INPUT_VERTEX_COORDINATE_NAME + ";\n");
    stringBuilder.append("  " + TEXTURE_COORDINATE_NAME + " = (" + TEXTURE_MATRIX_NAME + " * "
        + INPUT_TEXTURE_COORDINATE_NAME + ").xy;\n");
    stringBuilder.append("}\n");
    return stringBuilder.toString();
  }

  protected String createFragmentShaderString(ShaderType shaderType) {
    final StringBuilder stringBuilder = new StringBuilder();
    if (shaderType == ShaderType.OES) {
      stringBuilder.append("#extension GL_OES_EGL_image_external : require\n");
    }
    stringBuilder.append("precision " + precisionQualifierString(precisionQualifier) + " float;\n");
    stringBuilder.append("varying vec2 " + TEXTURE_COORDINATE_NAME + ";\n");

    if (shaderType == ShaderType.YUV) {
      stringBuilder.append("uniform sampler2D y_tex;\n");
      stringBuilder.append("uniform sampler2D u_tex;\n");
      stringBuilder.append("uniform sampler2D v_tex;\n");

      // Add separate function for sampling texture.
      stringBuilder.append("vec4 " + SAMPLE_FUNCTION_NAME + "(vec2 p) {\n"
          + "  float y = texture2D(y_tex, p).r;\n"
          + "  float u = texture2D(u_tex, p).r - 0.5;\n"
          + "  float v = texture2D(v_tex, p).r - 0.5;\n"
          + "  return vec4(y + 1.403 * v, y - 0.344 * u - 0.714 * v, y + 1.77 * u, 1);\n"
          + "}\n");
      stringBuilder.append(genericFragmentSource);
    } else {
      stringBuilder.append(
          "uniform " + samplerName(shaderType) + " " + textureVariableName(shaderType) + ";\n");

      // Update the sampling function in-place.
      stringBuilder.append(genericFragmentSource.replace(
          SAMPLE_FUNCTION_NAME + "(", "texture2D(" + textureVariableName(shaderType) + ", "));
    }

    return stringBuilder.toString();
  }

  protected GlShader createShader(ShaderType shaderType) {
    final GlShader shader =
        new GlShader(createVertexShaderString(), createFragmentShaderString(shaderType));
    shader.useProgram();

    if (shaderType == ShaderType.YUV) {
      GLES20.glUniform1i(shader.getUniformLocation("y_tex"), 0);
      GLES20.glUniform1i(shader.getUniformLocation("u_tex"), 1);
      GLES20.glUniform1i(shader.getUniformLocation("v_tex"), 2);
    } else {
      GLES20.glUniform1i(shader.getUniformLocation(textureVariableName(shaderType)), 0);
    }

    // Initialize vertex shader attributes.
    shader.setVertexAttribArray(INPUT_VERTEX_COORDINATE_NAME, 2, FULL_RECTANGLE_BUFFER);
    shader.setVertexAttribArray(INPUT_TEXTURE_COORDINATE_NAME, 2, FULL_RECTANGLE_TEXTURE_BUFFER);

    GlUtil.checkNoGLES2Error("Building shader");

    return shader;
  }
}
