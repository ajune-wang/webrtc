package org.webrtc;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import java.nio.FloatBuffer;
import java.util.IdentityHashMap;
import java.util.Map;
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;

public class GlBlurDrawer implements RendererCommon.GlDrawer {
  private static final String TAG = "GlBlurDrawer";
  // We will downscale frames (with proper box averaging) until the biggest side (width or height)
  // is this value.
  private static final int TARGET_SIDE = 80;

  private final GlGaussianBlur glFrameBufferBlur = new GlGaussianBlur();
  private final GlDownscaleDrawer glDownscaleDrawer = new GlDownscaleDrawer();
  private final GlRectDrawer glRectDrawer = new GlRectDrawer();
  private final VideoFrameDrawer frameDrawer = new VideoFrameDrawer();
  // List of frame buffers with increasing size (factor 2x), until it reaches 0.25x-0.5x of input
  // frame size. The frame buffers will have the same aspect as the frame size, and the smallest
  // frame buffer will have a width or height of TARGET_SIDE.
  private final List<GlTextureFrameBuffer> downscaleFrameBufferPyramid = new ArrayList<>();

  private void allocateFrameBuffers(int frameWidth, int frameHeight) {
    int width;
    int height;
    if (frameWidth > frameHeight) {
      width = TARGET_SIDE;
      height = (TARGET_SIDE * frameHeight) / frameWidth;
    } else {
      width = (TARGET_SIDE * frameWidth) / frameHeight;
      height = TARGET_SIDE;
    }

    Logging.d(TAG, "Allocating frame buffers");

    // Make an image pyramid. The highest frame buffer resolution will be 0.25x-0.5x of input
    // resolution in normal cases, but at least one frame buffer of minimum size will always be
    // allocated.
    int i = 0;
    do {
      // TODO(magjed): It might be inefficient that we resize the framebuffers when the rotation
      // changes.
      if (i >= downscaleFrameBufferPyramid.size()) {
        Logging.d(TAG, "Allocating new frame buffer with size: " + width + "x" + height);
        downscaleFrameBufferPyramid.add(new GlTextureFrameBuffer(GLES20.GL_RGBA));
      }
      Logging.d(TAG, "i = " + i + " size: " + width + "x" + height);
      downscaleFrameBufferPyramid.get(i).setSize(width, height);
      width *= 2;
      height *= 2;
      ++i;
    } while (width * 2 <= frameWidth);
  }

  // Returns the frame buffer that we will render into directly from the frame. This will be the
  // biggest, i.e. last, frame buffer in the pyramid.
  private GlTextureFrameBuffer getInputFrameBuffer() {
    return downscaleFrameBufferPyramid.get(downscaleFrameBufferPyramid.size() - 1);
  }

  // Will make sure necessary frame buffers are allocated, as well as setting the first frame buffer
  // as target, and deciding if we should start with a 2x downscaling or a simple texture sampling
  // to fill the first frame buffer.
  private RendererCommon.GlDrawer prepareRenderingAndGetInitialDrawer(
      int frameWidth, int frameHeight) {
    allocateFrameBuffers(frameWidth, frameHeight);
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, getInputFrameBuffer().getFrameBufferId());
    return (Math.max(frameWidth, frameHeight) >= TARGET_SIDE * 2) ? glDownscaleDrawer
                                                                  : glRectDrawer;
  }

  private void renderBlurredFrame(
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    Logging.d(TAG, "renderBlurredFrame");

    // Downscale the RGB textures from the frame buffers in a pyramid.
    for (int i = downscaleFrameBufferPyramid.size() - 1; i >= 1; --i) {
      final GlTextureFrameBuffer previousFrameBuffer = downscaleFrameBufferPyramid.get(i);
      final GlTextureFrameBuffer currentFrameBuffer = downscaleFrameBufferPyramid.get(i - 1);

      Logging.d(TAG,
          "Rendering i = " + i + " size: " + currentFrameBuffer.getWidth() + "x"
              + currentFrameBuffer.getHeight());

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, currentFrameBuffer.getFrameBufferId());

      glDownscaleDrawer.drawRgb(previousFrameBuffer.getTextureId(), RendererCommon.identityMatrix(),
          previousFrameBuffer.getWidth(), previousFrameBuffer.getHeight(), 0 /* viewportX */,
          0 /* viewportY */, currentFrameBuffer.getWidth(), currentFrameBuffer.getHeight());
    }

    // Blur the downscaled texture.
    final GlTextureFrameBuffer downscaledFrameBuffer = downscaleFrameBufferPyramid.get(0);
    for (int i = 0; i < 10; ++i) {
      glFrameBufferBlur.blur(downscaledFrameBuffer);
    }
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

    glRectDrawer.drawRgb(downscaledFrameBuffer.getTextureId(), RendererCommon.identityMatrix(),
        downscaledFrameBuffer.getWidth(), downscaledFrameBuffer.getHeight(), viewportX, viewportY,
        viewportWidth, viewportHeight);
  }

  @Override
  public void drawOes(int oesTextureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final RendererCommon.GlDrawer drawer =
        prepareRenderingAndGetInitialDrawer(frameWidth, frameHeight);
    drawer.drawOes(oesTextureId, texMatrix, frameWidth, frameHeight, 0 /* viewportX */,
        0 /* viewportY */, getInputFrameBuffer().getWidth(), getInputFrameBuffer().getHeight());
    renderBlurredFrame(viewportX, viewportY, viewportWidth, viewportHeight);
  }

  @Override
  public void drawRgb(int textureId, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final RendererCommon.GlDrawer drawer =
        prepareRenderingAndGetInitialDrawer(frameWidth, frameHeight);
    drawer.drawRgb(textureId, texMatrix, frameWidth, frameHeight, 0 /* viewportX */,
        0 /* viewportY */, getInputFrameBuffer().getWidth(), getInputFrameBuffer().getHeight());
    renderBlurredFrame(viewportX, viewportY, viewportWidth, viewportHeight);
  }

  @Override
  public void drawYuv(int[] yuvTextures, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight) {
    final RendererCommon.GlDrawer drawer =
        prepareRenderingAndGetInitialDrawer(frameWidth, frameHeight);
    drawer.drawYuv(yuvTextures, texMatrix, frameWidth, frameHeight, 0 /* viewportX */,
        0 /* viewportY */, getInputFrameBuffer().getWidth(), getInputFrameBuffer().getHeight());
    renderBlurredFrame(viewportX, viewportY, viewportWidth, viewportHeight);
  }

  @Override
  public void release() {
    for (GlTextureFrameBuffer frameBuffer : downscaleFrameBufferPyramid) {
      frameBuffer.release();
    }
    downscaleFrameBufferPyramid.clear();
    glDownscaleDrawer.release();
    glRectDrawer.release();
    frameDrawer.release();
    glFrameBufferBlur.release();
  }
}
