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
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import java.nio.FloatBuffer;
import org.webrtc.VideoFrameDrawer.YuvUploader;

/**
 * Generic GlVideoFrameDrawer implementation for rendering I420 / texture frames. Takes care of
 * common tasks such as shader caching and uploading YUV-buffers. For example usage see
 * RectGlVideoFrameDrawer.
 */
public class GenericGlVideoFrameDrawer implements GlVideoFrameDrawer {
  private static final float[] SRC_POINTS =
      new float[] {0f /* x0 */, 0f /* y0 */, 1f /* x1 */, 0f /* y1 */, 0f /* x2 */, 1f /* y2 */};

  // Vertex coordinates in Normalized Device Coordinates, i.e. (-1, -1) is bottom-left and (1, 1) is
  // top-right.
  private static final FloatBuffer FULL_RECTANGLE_BUF = GlUtil.createFloatBuffer(new float[] {
      -1.0f, -1.0f, // Bottom left.
      1.0f, -1.0f, // Bottom right.
      -1.0f, 1.0f, // Top left.
      1.0f, 1.0f, // Top right.
  });

  // Texture coordinates - (0, 0) is bottom-left and (1, 1) is top-right.
  private static final FloatBuffer FULL_RECTANGLE_TEX_BUF = GlUtil.createFloatBuffer(new float[] {
      0.0f, 0.0f, // Bottom left.
      1.0f, 0.0f, // Bottom right.
      0.0f, 1.0f, // Top left.
      1.0f, 1.0f // Top right.
  });

  public static class FrameInformation {
    public Matrix renderMatrix;
    public final int renderWidth;
    public final int renderHeight;

    FrameInformation(Matrix renderMatrix, int renderWidth, int renderHeight) {
      this.renderMatrix = renderMatrix;
      this.renderWidth = renderWidth;
      this.renderHeight = renderHeight;
    }
  }

  /**
   * Describes format of a frame (i.e. YUV, RGB, OES). When the frame format changes, the shader is
   * recompiled.
   */
  public static class FrameFormat {
    private final boolean isYuv;
    private final VideoFrame.TextureBuffer.Type textureType;

    private FrameFormat(boolean isYuv, VideoFrame.TextureBuffer.Type textureType) {
      this.isYuv = isYuv;
      this.textureType = textureType;
    }

    /** Is this a frame with separete Y-, U- and V-textures? */
    public boolean isYuv() {
      return isYuv;
    }

    /** Returns the texture type if this is a texture frame, otherwise null. */
    public VideoFrame.TextureBuffer.Type getTextureType() {
      return textureType;
    }

    static FrameFormat forTextureType(VideoFrame.TextureBuffer.Type textureType) {
      return new FrameFormat(false /* isYuv */, textureType);
    }

    static FrameFormat forI420() {
      return new FrameFormat(true /* isYuv */, null /* textureType */);
    }
  }

  public interface ShaderDefinition {
    /**
     * Returns a GlShader that is suitable for rendering the given FrameFormat. The shader will be
     * cached by the caller.
     *
     * <p>
     * Input variables:
     * <ul>
     *  <li>in_pos - The input vertex coordinates</li>
     *  <li>in_tc - The input texture coordinates</li>
     * </ul>
     *
     * <p>
     * For texture frames, the texture will be set to tex uniform. For YUV-frames, the plane
     * textures will be set to y_tex, u_tex and v_tex uniforms.
     *
     * @return A compiled GlShader, the caller takes ownership of this object.
     */
    GlShader getShader(FrameFormat frameFormat);

    /** Does any custom preparation on the shader such as setting static uniforms. */
    void prepareShader(FrameFormat frameFormat, GlShader shader);

    /** Updates the shader with information from the current frame. */
    void updateShader(FrameFormat frameFormat, GlShader shader, FrameInformation frameInformation);
  }

  private final YuvUploader yuvUploader = new YuvUploader();
  private final ShaderDefinition shaderDefinition;
  private FrameFormat frameFormat;
  private GlShader shader;
  private int texMatrixUniform;
  private VideoFrame.Buffer lastI420Buffer;

  public GenericGlVideoFrameDrawer(ShaderDefinition shaderDefinition) {
    this.shaderDefinition = shaderDefinition;
  }

  @Override
  public void drawFrame(VideoFrame frame, Matrix additionalRenderMatrix, int viewportX,
      int viewportY, int viewportWidth, int viewportHeight) {
    final FrameInformation frameInformation = getFrameInformation(frame, additionalRenderMatrix);

    if (frame.getBuffer() instanceof VideoFrame.TextureBuffer) {
      drawTexture((VideoFrame.TextureBuffer) frame.getBuffer(), frameInformation, viewportX,
          viewportY, viewportWidth, viewportHeight);
    } else {
      drawI420(
          frame.getBuffer(), frameInformation, viewportX, viewportY, viewportWidth, viewportHeight);
    }
  }

  @Override
  public void dispose() {
    yuvUploader.release();
    frameFormat = null;
    if (shader != null) {
      shader.release();
      shader = null;
    }
    lastI420Buffer = null;
  }

  private static FrameInformation getFrameInformation(
      VideoFrame frame, Matrix additionalRenderMatrix) {
    final int frameWidth = frame.getRotatedWidth();
    final int frameHeight = frame.getRotatedHeight();

    final int renderWidth;
    final int renderHeight;
    if (additionalRenderMatrix == null) {
      renderWidth = frameWidth;
      renderHeight = frameHeight;
    } else {
      float[] dstPoints = new float[6];
      // Transform the texture coordinates (in the range [0, 1]) according to |renderMatrix|.
      additionalRenderMatrix.mapPoints(dstPoints, SRC_POINTS);

      // Multiply with the width and height to get the positions in terms of pixels.
      for (int i = 0; i < 3; ++i) {
        dstPoints[i * 2 + 0] *= frameWidth;
        dstPoints[i * 2 + 1] *= frameHeight;

        // Logging.d("POINT", "x: " + dstPoints[i * 2 + 0] + ", y: " + dstPoints[i * 2 + 1]);
      }

      // Get the length of the sides of the transformed rectangle in terms of pixels.
      renderWidth = distance(dstPoints[0], dstPoints[1], dstPoints[2], dstPoints[3]);
      renderHeight = distance(dstPoints[0], dstPoints[1], dstPoints[4], dstPoints[5]);
    }

    final boolean isTextureFrame = frame.getBuffer() instanceof VideoFrame.TextureBuffer;
    Matrix renderMatrix = new Matrix();
    if (isTextureFrame) {
      renderMatrix.preConcat(((VideoFrame.TextureBuffer) frame.getBuffer()).getTransformMatrix());
    }
    renderMatrix.preTranslate(0.5f, 0.5f);
    if (!isTextureFrame) {
      renderMatrix.preScale(1f, -1f); // I420-frames are upside down
    }
    renderMatrix.preRotate(frame.getRotation());
    renderMatrix.preTranslate(-0.5f, -0.5f);
    if (additionalRenderMatrix != null) {
      renderMatrix.preConcat(additionalRenderMatrix);
    }

    return new FrameInformation(renderMatrix, renderWidth, renderHeight);
  }

  private static int distance(float x0, float y0, float x1, float y1) {
    return (int) Math.round(Math.hypot(x1 - x0, y1 - y0));
  }

  private void drawTexture(VideoFrame.TextureBuffer buffer, FrameInformation frameInformation,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final VideoFrame.TextureBuffer.Type bufferType = buffer.getType();
    final int glTarget = bufferType.getGlTarget();

    prepareTextureShader(bufferType);
    shader.useProgram();
    updateShader(shader, frameInformation);

    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(glTarget, buffer.getTextureId());
    drawRectangle(viewportX, viewportY, viewportWidth, viewportHeight);
    // Unbind the texture as a precaution.
    GLES20.glBindTexture(glTarget, 0);
    GlUtil.checkNoGLES2Error("drawTexture");
  }

  private void prepareTextureShader(VideoFrame.TextureBuffer.Type textureType) {
    if (frameFormat != null && textureType == frameFormat.getTextureType()) {
      return; // We already have the correct type of shader.
    }
    if (shader != null) {
      shader.release();
    }
    frameFormat = FrameFormat.forTextureType(textureType);
    prepareShaderCommon();
    GLES20.glUniform1i(shader.getUniformLocation("tex"), 0);
    GlUtil.checkNoGLES2Error("prepareTextureShader");
  }

  private void drawI420(VideoFrame.Buffer buffer, FrameInformation frameInformation, int viewportX,
      int viewportY, int viewportWidth, int viewportHeight) {
    prepareI420Shader();
    updateShader(shader, frameInformation);
    shader.useProgram();

    if (buffer != lastI420Buffer) {
      VideoFrame.I420Buffer i420Buffer = buffer.toI420();
      yuvUploader.uploadFromBuffer(i420Buffer);
      i420Buffer.release();
    }

    int[] yuvTextures = yuvUploader.getYuvTextures();
    // Bind the textures.
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
    }
    drawRectangle(viewportX, viewportY, viewportWidth, viewportHeight);
    // Unbind the textures as a precaution.
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }
    GlUtil.checkNoGLES2Error("drawI420");
  }

  private void prepareI420Shader() {
    if (frameFormat != null && frameFormat.isYuv()) {
      return; // We already have the correct type of shader.
    }
    if (shader != null) {
      shader.release();
    }
    frameFormat = FrameFormat.forI420();
    prepareShaderCommon();
    GLES20.glUniform1i(shader.getUniformLocation("y_tex"), 0);
    GLES20.glUniform1i(shader.getUniformLocation("u_tex"), 1);
    GLES20.glUniform1i(shader.getUniformLocation("v_tex"), 2);
    GlUtil.checkNoGLES2Error("prepareI420Shader");
  }

  private void prepareShaderCommon() {
    shader = shaderDefinition.getShader(frameFormat);
    shader.useProgram();
    texMatrixUniform = shader.getUniformLocation("tex_matrix");
    shader.setVertexAttribArray("in_pos", 2, FULL_RECTANGLE_BUF);
    shader.setVertexAttribArray("in_tc", 2, FULL_RECTANGLE_TEX_BUF);
    shaderDefinition.prepareShader(frameFormat, shader);
    GlUtil.checkNoGLES2Error("prepareShaderCommon");
  }

  private void updateShader(GlShader shader, FrameInformation frameInformation) {
    GLES20.glUniformMatrix4fv(texMatrixUniform, 1 /* count */, false /* transpose */,
        RendererCommon.convertMatrixFromAndroidGraphicsMatrix(frameInformation.renderMatrix),
        0 /* offset */);
    shaderDefinition.updateShader(frameFormat, shader, frameInformation);
    GlUtil.checkNoGLES2Error("updateShader");
  }

  private void drawRectangle(int x, int y, int width, int height) {
    // Draw quad.
    GLES20.glViewport(x, y, width, height);
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
  }
}
