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

/**
 * This class is a helper class for building instances of RendererCommon.GlDrawer given a
 * GlShaderBuilder.
 */
class GlGenericDrawer implements RendererCommon.GlDrawer {
  private final GlShaderBuilder shaderBuilder;
  private final GlShaderBuilder.ShaderCallbacks shaderCallbacks;

  private GlShaderBuilder.ShaderType currentShaderType;
  private GlShader currentShader;
  private int texMatrixLocation;

  public GlGenericDrawer(
      GlShaderBuilder shaderBuilder, GlShaderBuilder.ShaderCallbacks shaderCallbacks) {
    this.shaderBuilder = shaderBuilder;
    this.shaderCallbacks = shaderCallbacks;
  }

  /**
   * Draw an OES texture frame with specified texture transformation matrix. Required resources
   * are allocated at the first call to this function.
   */
  @Override
  public void drawOes(int oesTextureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    prepareShader(GlShaderBuilder.ShaderType.OES, texMatrix, frameWidth, frameHeight, viewportWidth,
        viewportHeight);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    // updateTexImage() may be called from another thread in another EGL context, so we need to
    // bind/unbind the texture in each draw call so that GLES understads it's a new texture.
    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oesTextureId);
    GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
  }

  /**
   * Draw a RGB(A) texture frame with specified texture transformation matrix. Required resources
   * are allocated at the first call to this function.
   */
  @Override
  public void drawRgb(int textureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    prepareShader(GlShaderBuilder.ShaderType.RGB, texMatrix, frameWidth, frameHeight, viewportWidth,
        viewportHeight);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
    GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    // Unbind the texture as a precaution.
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
  }

  /**
   * Draw a YUV frame with specified texture transformation matrix. Required resources are
   * allocated at the first call to this function.
   */
  @Override
  public void drawYuv(int[] yuvTextures, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    prepareShader(GlShaderBuilder.ShaderType.YUV, texMatrix, frameWidth, frameHeight, viewportWidth,
        viewportHeight);
    // Bind the textures.
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
    }
    GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    // Unbind the textures as a precaution..
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }
  }

  private void prepareShader(GlShaderBuilder.ShaderType shaderType, float[] texMatrix,
      int frameWidth, int frameHeight, int viewportWidth, int viewportHeight) {
    if (!shaderType.equals(currentShaderType)) {
      currentShaderType = shaderType;
      if (currentShader != null) {
        currentShader.release();
      }
      currentShader = shaderBuilder.createShader(shaderType);
      shaderCallbacks.onNewShader(shaderType, currentShader);
      texMatrixLocation = currentShader.getUniformLocation(GlShaderBuilder.TEXTURE_MATRIX_NAME);
    }
    currentShader.useProgram();
    // Copy the texture transformation matrix over.
    GLES20.glUniformMatrix4fv(texMatrixLocation, 1, false, texMatrix, 0);
    shaderCallbacks.onPrepareShader(shaderType, currentShader, texMatrix, frameWidth, frameHeight,
        viewportWidth, viewportHeight);
  }

  /**
   * Release all GLES resources. This needs to be done manually, otherwise the resources are
   * leaked.
   */
  @Override
  public void release() {
    if (currentShader != null) {
      currentShader = null;
      currentShaderType = null;
    }
  }
}
