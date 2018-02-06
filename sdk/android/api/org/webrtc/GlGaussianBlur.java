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
public class GlGaussianBlur {
  private static final String VERTEX_SHADER = "varying vec2 tc;\n"
      + "attribute vec4 in_pos;\n"
      + "attribute vec4 in_tc;\n"
      + "\n"
      + "void main() {\n"
      + "    gl_Position = in_pos;\n"
      + "    tc = in_tc.xy;\n"
      + "}\n";

  private static final int KERNEL_RADIUS = 5;

  private static final String FRAGMENT_SHADER = "precision mediump float;\n"
      + "varying vec2 tc;\n"
      + "\n"
      + "uniform sampler2D tex;\n"
      + "uniform vec2 blurDir;\n"
      // The hardcoded coefficients must correspond to KERNEL_RADIUS.
      + "uniform float c0;\n"
      + "uniform float c1;\n"
      + "uniform float c2;\n"
      + "uniform float c3;\n"
      + "uniform float c4;\n"
      + "uniform float c5;\n"
      + "\n"
      + "void main() {\n"
      + "  gl_FragColor =\n"
      + "    + (c0 * texture2D(tex, tc)\n"
      + "    + c1 * (texture2D(tex, tc - blurDir) + texture2D(tex, tc + blurDir)))\n"
      + "    + (c2 * (texture2D(tex, tc - 2.0 * blurDir) + texture2D(tex, tc + 2.0 * blurDir))"
      + "    + c3 * (texture2D(tex, tc - 3.0 * blurDir) + texture2D(tex, tc + 3.0 * blurDir)))\n"
      + "    + (c4 * (texture2D(tex, tc - 4.0 * blurDir) + texture2D(tex, tc + 4.0 * blurDir))\n"
      + "    + c5 * (texture2D(tex, tc - 5.0 * blurDir) + texture2D(tex, tc + 5.0 * blurDir)));\n"
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

  // Holds a semi-blurred frame that is blurred in one direction.
  private GlTextureFrameBuffer blurTmpFrameBuffer;
  private GlShader glShader;
  private int blurDirLoc;

  // 0.214607

  private GlShader compileGaussianBlurShader(float sigma) {
    final GlShader glShader = new GlShader(VERTEX_SHADER, FRAGMENT_SHADER);
    glShader.useProgram();
    glShader.setVertexAttribArray("in_pos", 2, FULL_RECTANGLE_BUF);
    glShader.setVertexAttribArray("in_tc", 2, FULL_RECTANGLE_TEX_BUF);
    blurDirLoc = glShader.getUniformLocation("blurDir");
    GLES20.glUniform1i(glShader.getUniformLocation("tex"), 0);
    GlUtil.checkNoGLES2Error("Initialize shader.");

    final float twoSigmaSquare = 2.0f * sigma * sigma;
    final float coefficients[] = new float[KERNEL_RADIUS + 1];
    float sum = 0.0f;
    for (int i = 0; i <= KERNEL_RADIUS; ++i) {
      final float c = (float) Math.exp(-i * i / twoSigmaSquare);
      coefficients[i] = c;
      sum += (i == 0) ? c : (2 * c);
    }
    for (int i = 0; i <= KERNEL_RADIUS; ++i) {
      coefficients[i] /= sum;
    }

    for (int i = 0; i <= KERNEL_RADIUS; ++i) {
      Logging.e("###", "Coefficients " + i + ": " + coefficients[i]);
    }

    for (int i = 0; i <= KERNEL_RADIUS; ++i) {
      GLES20.glUniform1f(glShader.getUniformLocation("c" + i), coefficients[i]);
    }

    return glShader;
  }

  /**
   * Blur in-place.
   */
  public void blur(GlTextureFrameBuffer frameBuffer) {
    if (glShader == null) {
      glShader = compileGaussianBlurShader(2.0f /* sigma */);
    }
    if (blurTmpFrameBuffer == null) {
      blurTmpFrameBuffer = new GlTextureFrameBuffer(GLES20.GL_RGBA);
    }
    blurTmpFrameBuffer.setSize(frameBuffer.getWidth(), frameBuffer.getHeight());

    glShader.useProgram();
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glViewport(0 /* x */, 0 /* y */, frameBuffer.getWidth(), frameBuffer.getHeight());

    // Blur in x-direction first, to our temporary blur buffer.
    GLES20.glUniform2f(blurDirLoc, 1.0f / frameBuffer.getWidth(), 0.0f);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, frameBuffer.getTextureId());

    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, blurTmpFrameBuffer.getFrameBufferId());
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

    // Blur in y-direction, back to the passed in frame buffer.
    GLES20.glUniform2f(blurDirLoc, 0.0f, 1.0f / frameBuffer.getHeight());
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, blurTmpFrameBuffer.getTextureId());

    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, frameBuffer.getFrameBufferId());
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

    // Unbind the texture and frame buffer as a precaution.
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
  }

  /**
   * Release all GLES resources. This needs to be done manually, otherwise the resources are leaked.
   */
  public void release() {
    if (glShader != null) {
      glShader.release();
    }
  }
}
