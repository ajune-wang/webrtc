package org.webrtc;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import java.nio.FloatBuffer;
import java.util.IdentityHashMap;
import java.util.Map;

/**
 * 2x box filter downscaling. Viewport size should preferable be half the width and half the height
 * of the texture size for optimal results, but reasonable deviations from this will still generate
 * acceptable outputs.
 */
// TODO(magjed): Should we use glGenerateMipmap() instead? Is it always available?
public class GlDownscaleDrawer implements RendererCommon.GlDrawer {
  // Simple vertex shader, used for both YUV and OES.
  private static final String VERTEX_SHADER = "varying vec2 tc;\n"
      + "attribute vec4 in_pos;\n"
      + "attribute vec4 in_tc;\n"
      + "\n"
      + "uniform mat4 texMatrix;\n"
      + "\n"
      + "void main() {\n"
      + "    gl_Position = in_pos;\n"
      + "    tc = (texMatrix * in_tc).xy;\n"
      + "}\n";

  private static final String RGB_FRAGMENT_SHADER = "precision mediump float;\n"
      + "varying vec2 tc;\n"
      + "\n"
      + "uniform sampler2D tex;\n"
      // Difference in texture coordinate corresponding to one pixel in the x/y direction.
      + "uniform vec2 xUnit;\n"
      + "uniform vec2 yUnit;\n"
      + "\n"
      + "void main() {\n"
      + "  gl_FragColor = 0.25 * (texture2D(tex, tc - xUnit - yUnit) + texture2D(tex, tc + xUnit - yUnit) + texture2D(tex, tc - xUnit + yUnit) + texture2D(tex, tc + xUnit + yUnit));\n"
      + "}\n";

  private static final String YUV_FRAGMENT_SHADER = "";

  private static final String OES_FRAGMENT_SHADER =
      "#extension GL_OES_EGL_image_external : require\n"
      + "precision mediump float;\n"
      + "varying vec2 tc;\n"
      + "\n"
      + "uniform samplerExternalOES tex;\n"
      // Difference in texture coordinate corresponding to one pixel in the x/y direction.
      + "uniform vec2 xUnit;\n"
      + "uniform vec2 yUnit;\n"
      + "\n"
      + "void main() {\n"
      + "  gl_FragColor = 0.25 * (texture2D(tex, tc - xUnit - yUnit) + texture2D(tex, tc + xUnit - yUnit) + texture2D(tex, tc - xUnit + yUnit) + texture2D(tex, tc + xUnit + yUnit));\n"
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

  private static class Shader {
    public final GlShader glShader;
    public final int texMatrixLocation;
    public final int xUnitLoc;
    public final int yUnitLoc;

    public Shader(String fragmentShader) {
      this.glShader = new GlShader(VERTEX_SHADER, fragmentShader);
      glShader.useProgram();
      glShader.setVertexAttribArray("in_pos", 2, FULL_RECTANGLE_BUF);
      glShader.setVertexAttribArray("in_tc", 2, FULL_RECTANGLE_TEX_BUF);
      this.xUnitLoc = glShader.getUniformLocation("xUnit");
      this.yUnitLoc = glShader.getUniformLocation("yUnit");
      this.texMatrixLocation = glShader.getUniformLocation("texMatrix");
      GlUtil.checkNoGLES2Error("Initialize shader.");
    }
  }

  private Shader oesShader;
  private Shader rgbShader;
  private Shader yuvShader;

  /**
   * Draw an OES texture frame with specified texture transformation matrix. Required resources are
   * allocated at the first call to this function.
   */
  @Override
  public void drawOes(int oesTextureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    if (oesShader == null) {
      oesShader = new Shader(OES_FRAGMENT_SHADER);
      GLES20.glUniform1i(oesShader.glShader.getUniformLocation("tex"), 0);
    }
    prepareShader(oesShader, texMatrix, viewportWidth, viewportHeight);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    // updateTexImage() may be called from another thread in another EGL context, so we need to
    // bind/unbind the texture in each draw call so that GLES understads it's a new texture.
    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oesTextureId);
    drawRectangle(viewportX, viewportY, viewportWidth, viewportHeight);
    GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
  }

  /**
   * Draw a RGB(A) texture frame with specified texture transformation matrix. Required resources
   * are allocated at the first call to this function.
   */
  @Override
  public void drawRgb(int textureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    if (rgbShader == null) {
      rgbShader = new Shader(RGB_FRAGMENT_SHADER);
      GLES20.glUniform1i(rgbShader.glShader.getUniformLocation("tex"), 0);
    }
    prepareShader(rgbShader, texMatrix, viewportWidth, viewportHeight);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
    drawRectangle(viewportX, viewportY, viewportWidth, viewportHeight);
    // Unbind the texture as a precaution.
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
  }

  /**
   * Draw a YUV frame with specified texture transformation matrix. Required resources are allocated
   * at the first call to this function.
   */
  @Override
  public void drawYuv(int[] yuvTextures, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    if (yuvShader == null) {
      yuvShader = new Shader(YUV_FRAGMENT_SHADER);
      GLES20.glUniform1i(yuvShader.glShader.getUniformLocation("y_tex"), 0);
      GLES20.glUniform1i(yuvShader.glShader.getUniformLocation("u_tex"), 1);
      GLES20.glUniform1i(yuvShader.glShader.getUniformLocation("v_tex"), 2);
    }
    prepareShader(yuvShader, texMatrix, viewportWidth, viewportHeight);
    // Bind the textures.
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
    }
    drawRectangle(viewportX, viewportY, viewportWidth, viewportHeight);
    // Unbind the textures as a precaution..
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }
  }

  private void drawRectangle(int x, int y, int width, int height) {
    // Draw quad.
    GLES20.glViewport(x, y, width, height);
    GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
  }

  private void prepareShader(Shader shader, float[] texMatrix, int outputWidth, int outputHeight) {
    shader.glShader.useProgram();
    // Copy the texture transformation matrix over.
    GLES20.glUniformMatrix4fv(shader.texMatrixLocation, 1, false, texMatrix, 0);
    // Matrix * (0.25;0;0;0) / outputWidth and Matrix * (0;1;0;0) / height. Note that opengl uses
    // column major order.
    final float xFactor = 0.25f / outputWidth;
    final float yFactor = 0.25f / outputHeight;
    GLES20.glUniform2f(shader.xUnitLoc, texMatrix[0] * xFactor, texMatrix[1] * xFactor);
    GLES20.glUniform2f(shader.yUnitLoc, texMatrix[4] * yFactor, texMatrix[5] * yFactor);
  }

  /**
   * Release all GLES resources. This needs to be done manually, otherwise the resources are leaked.
   */
  @Override
  public void release() {
    if (oesShader != null) {
      oesShader.glShader.release();
    }
    if (rgbShader != null) {
      rgbShader.glShader.release();
    }
    if (yuvShader != null) {
      yuvShader.glShader.release();
    }
  }
}
