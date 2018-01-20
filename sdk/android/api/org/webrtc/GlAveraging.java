package org.webrtc;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import java.nio.FloatBuffer;
import java.util.IdentityHashMap;
import java.util.Map;

/**
 * This class performs Gaussian blurring with a 7x separable kernel from an RGB framebuffer to
 * another RGB framebuffer.
 */
public class GlAveraging {
  private static final String VERTEX_SHADER = "varying vec2 tc;\n"
      + "attribute vec4 in_pos;\n"
      + "attribute vec4 in_tc;\n"
      + "\n"
      + "void main() {\n"
      + "    gl_Position = in_pos;\n"
      + "    tc = in_tc.xy;\n"
      + "}\n";

  private static final String FRAGMENT_SHADER = "precision mediump float;\n"
      + "varying vec2 tc;\n"
      + "\n"
      + "uniform sampler2D texA;\n"
      + "uniform sampler2D texB;\n"
      + "\n"
      + "void main() {\n"
      + "  gl_FragColor = texture2D(texA, tc) * 0.1 + texture2D(texB, tc) * 0.9;\n"
      + "}\n";

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

  private GlShader glShader;
  private GlTextureFrameBuffer frameBufferA;
  private GlTextureFrameBuffer frameBufferB;

  private GlShader compileShader() {
    final GlShader glShader = new GlShader(VERTEX_SHADER, FRAGMENT_SHADER);
    glShader.useProgram();
    glShader.setVertexAttribArray("in_pos", 2, FULL_RECTANGLE_BUF);
    glShader.setVertexAttribArray("in_tc", 2, FULL_RECTANGLE_TEX_BUF);
    GLES20.glUniform1i(glShader.getUniformLocation("texA"), 0);
    GLES20.glUniform1i(glShader.getUniformLocation("texB"), 1);
    GlUtil.checkNoGLES2Error("Initialize shader.");
    return glShader;
  }

  /**
   * Blur in-place.
   */
  public void average(GlTextureFrameBuffer frameBuffer) {
    if (glShader == null) {
      glShader = compileShader();
    }
    if (frameBufferA == null) {
      frameBufferA = new GlTextureFrameBuffer(GLES20.GL_RGBA);
    }
    frameBufferA.setSize(frameBuffer.getWidth(), frameBuffer.getHeight());

    if (frameBufferB == null) {
      frameBufferB = new GlTextureFrameBuffer(GLES20.GL_RGBA);
    }
    frameBufferB.setSize(frameBuffer.getWidth(), frameBuffer.getHeight());

    glShader.useProgram();
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, frameBuffer.getTextureId());

    GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, frameBufferA.getTextureId());

    GLES20.glViewport(0 /* x */, 0 /* y */, frameBufferB.getWidth(), frameBufferB.getHeight());

    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, frameBufferB.getFrameBufferId());
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);

    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);

    GlTextureFrameBuffer tmp = frameBufferA;
    frameBufferA = frameBufferB;
    frameBufferB = tmp;
  }

  public int getTextureId() {
    return frameBufferB.getTextureId();
  }

  /**
   * Release all GLES resources. This needs to be done manually, otherwise the resources are leaked.
   */
  public void release() {
    if (glShader != null) {
      glShader.release();
    }
    if (frameBufferA != null) {
      frameBufferA.release();
    }
    if (frameBufferB != null) {
      frameBufferB.release();
    }
  }
}
