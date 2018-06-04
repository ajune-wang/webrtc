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
import android.opengl.GLES20;
import java.nio.ByteBuffer;
import org.webrtc.VideoFrame.I420Buffer;
import org.webrtc.VideoFrame.TextureBuffer;

/**
 * Class for converting OES textures to a YUV ByteBuffer. It can be constructed on any thread, but
 * should only be operated from a single thread with an active EGL context.
 */
public class YuvConverter {
  private static final String FRAGMENT_SHADER =
      // Difference in texture coordinate corresponding to one
      // sub-pixel in the x direction.
      "uniform vec2 xUnit;\n"
      // Color conversion coefficients, including constant term
      + "uniform vec4 coeffs;\n"
      + "\n"
      + "void main() {\n"
      // Since the alpha read from the texture is always 1, this could
      // be written as a mat4 x vec4 multiply. However, that seems to
      // give a worse framerate, possibly because the additional
      // multiplies by 1.0 consume resources. TODO(nisse): Could also
      // try to do it as a vec3 x mat3x4, followed by an add in of a
      // constant vector.
      + "  gl_FragColor.r = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc - 1.5 * xUnit).rgb);\n"
      + "  gl_FragColor.g = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc - 0.5 * xUnit).rgb);\n"
      + "  gl_FragColor.b = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc + 0.5 * xUnit).rgb);\n"
      + "  gl_FragColor.a = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc + 1.5 * xUnit).rgb);\n"
      + "}\n";

  private static class ShaderCallbacks implements GlGenericDrawer.ShaderCallbacks {
    // Y'UV444 to RGB888, see https://en.wikipedia.org/wiki/YUV#Y.27UV444_to_RGB888_conversion. We
    // use the ITU-R coefficients for U and V.
    private static final float[] yCoeffs = new float[] {0.299f, 0.587f, 0.114f, 0.0f};
    private static final float[] uCoeffs = new float[] {-0.169f, -0.331f, 0.499f, 0.5f};
    private static final float[] vCoeffs = new float[] {0.499f, -0.418f, -0.0813f, 0.5f};

    private int xUnitLoc;
    private int coeffsLoc;

    private float[] coeffs;
    private float stepSize;

    public void setPlaneY() {
      coeffs = yCoeffs;
      stepSize = 1.0f;
    }

    public void setPlaneU() {
      coeffs = uCoeffs;
      stepSize = 2.0f;
    }

    public void setPlaneV() {
      coeffs = vCoeffs;
      stepSize = 2.0f;
    }

    @Override
    public void onNewShader(GlShader shader) {
      xUnitLoc = shader.getUniformLocation("xUnit");
      coeffsLoc = shader.getUniformLocation("coeffs");
    }

    @Override
    public void onPrepareShader(GlShader shader, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportWidth, int viewportHeight) {
      GLES20.glUniform4f(coeffsLoc, coeffs[0], coeffs[1], coeffs[2], coeffs[3]);
      // Matrix * (1;0;0;0) / (width / stepSize). Note that opengl uses column major order.
      GLES20.glUniform2f(
          xUnitLoc, stepSize * texMatrix[0] / frameWidth, stepSize * texMatrix[1] / frameWidth);
    }
  }

  private final ThreadUtils.ThreadChecker threadChecker = new ThreadUtils.ThreadChecker();
  private final GlTextureFrameBuffer textureFrameBuffer = new GlTextureFrameBuffer(GLES20.GL_RGBA);
  private boolean released = false;
  private final ShaderCallbacks shaderCallbacks = new ShaderCallbacks();
  private final GlGenericDrawer drawer = new GlGenericDrawer(FRAGMENT_SHADER, shaderCallbacks);

  /**
   * This class should be constructed on a thread that has an active EGL context.
   */
  public YuvConverter() {
    threadChecker.detachThread();
  }

  /** Converts the texture buffer to I420. */
  public I420Buffer convert(TextureBuffer textureBuffer) {
    threadChecker.checkIsOnValidThread();
    if (released) {
      throw new IllegalStateException("YuvConverter.convert called on released object");
    }
    final int width = textureBuffer.getWidth();
    final int height = textureBuffer.getHeight();

    // SurfaceTextureHelper requires a stride that is divisible by 8.  Round width up.
    // See SurfaceTextureHelper for details on the size and format.
    final int stride = ((width + 7) / 8) * 8;
    final int uvHeight = (height + 1) / 2;
    final int totalHeight = height + uvHeight;
    // Due to the layout used by SurfaceTextureHelper, vPos + stride * uvHeight would overrun the
    // buffer.  Add one row at the bottom to compensate for this.  There will never be data in the
    // extra row, but now other code does not have to deal with v stride * v height exceeding the
    // buffer's capacity.
    final int size = stride * (totalHeight + 1);
    ByteBuffer i420ByteBuffer = JniCommon.nativeAllocateByteBuffer(size);

    // We draw into a buffer laid out like
    //
    //    +---------+
    //    |         |
    //    |  Y      |
    //    |         |
    //    |         |
    //    +----+----+
    //    | U  | V  |
    //    |    |    |
    //    +----+----+
    //
    // In memory, we use the same stride for all of Y, U and V. The
    // U data starts at offset |height| * |stride| from the Y data,
    // and the V data starts at at offset |stride/2| from the U
    // data, with rows of U and V data alternating.
    //
    // Now, it would have made sense to allocate a pixel buffer with
    // a single byte per pixel (EGL10.EGL_COLOR_BUFFER_TYPE,
    // EGL10.EGL_LUMINANCE_BUFFER,), but that seems to be
    // unsupported by devices. So do the following hack: Allocate an
    // RGBA buffer, of width |stride|/4. To render each of these
    // large pixels, sample the texture at 4 different x coordinates
    // and store the results in the four components.
    //
    // Since the V data needs to start on a boundary of such a
    // larger pixel, it is not sufficient that |stride| is even, it
    // has to be a multiple of 8 pixels.
    final int yWidth = (width + 3) / 4;
    final int uvWidth = (width + 7) / 8;

    // Produce a frame buffer starting at top-left corner, not bottom-left.
    final Matrix renderMatrix = new Matrix();
    renderMatrix.preTranslate(0.5f, 0.5f);
    renderMatrix.preScale(1f, -1f);
    renderMatrix.preTranslate(-0.5f, -0.5f);

    textureFrameBuffer.setSize(stride / 4, totalHeight);

    // Bind our framebuffer.
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, textureFrameBuffer.getFrameBufferId());
    GlUtil.checkNoGLES2Error("glBindFramebuffer");

    // Draw Y.
    shaderCallbacks.setPlaneY();
    VideoFrameDrawer.drawTexture(drawer, textureBuffer, renderMatrix, width, height,
        /* viewportX */ 0, /* viewportY */ 0, yWidth, height);

    // Draw U.
    shaderCallbacks.setPlaneU();
    VideoFrameDrawer.drawTexture(drawer, textureBuffer, renderMatrix, width, height,
        /* viewportX */ 0, height, uvWidth, uvHeight);

    // Draw V.
    shaderCallbacks.setPlaneV();
    VideoFrameDrawer.drawTexture(drawer, textureBuffer, renderMatrix, width, height,
        /* viewportX */ stride / 8, height, uvWidth, uvHeight);

    GLES20.glReadPixels(0, 0, textureFrameBuffer.getWidth(), textureFrameBuffer.getHeight(),
        GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, i420ByteBuffer);

    GlUtil.checkNoGLES2Error("YuvConverter.convert");

    // Restore normal framebuffer.
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

    // Prepare Y, U, and V ByteBuffer slices.
    final int yPos = 0;
    final int uPos = yPos + stride * height;
    // Rows of U and V alternate in the buffer, so V data starts after the first row of U.
    final int vPos = uPos + stride / 2;

    i420ByteBuffer.position(yPos);
    i420ByteBuffer.limit(yPos + stride * height);
    ByteBuffer dataY = i420ByteBuffer.slice();

    i420ByteBuffer.position(uPos);
    i420ByteBuffer.limit(uPos + stride * uvHeight);
    ByteBuffer dataU = i420ByteBuffer.slice();

    i420ByteBuffer.position(vPos);
    i420ByteBuffer.limit(vPos + stride * uvHeight);
    ByteBuffer dataV = i420ByteBuffer.slice();

    // SurfaceTextureHelper uses the same stride for Y, U, and V data.
    return JavaI420Buffer.wrap(width, height, dataY, stride, dataU, stride, dataV, stride,
        () -> { JniCommon.nativeFreeByteBuffer(i420ByteBuffer); });
  }

  public void release() {
    threadChecker.checkIsOnValidThread();
    released = true;
    drawer.release();
    textureFrameBuffer.release();
  }
}
