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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import java.nio.ByteBuffer;
import java.util.Random;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.VideoFrame.I420Buffer;
import org.webrtc.VideoFrame.TextureBuffer;

@RunWith(BaseJUnit4ClassRunner.class)
public class RectGlVideoFrameDrawerTest {
  // Resolution of the test image.
  private static final int WIDTH = 16;
  private static final int HEIGHT = 16;

  private final Random random = new Random(42);
  private EglBase eglBase;
  private ByteBuffer expectedRgbData;
  private int rgbTexture;
  private VideoFrame rgbFrame;
  private RectGlVideoFrameDrawer drawer;

  @Before
  public void setUp() {
    // Create EGL base with a pixel buffer as display output.
    eglBase = EglBase.create(null, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createPbufferSurface(WIDTH, HEIGHT);
    eglBase.makeCurrent();

    // Create RGB byte buffer plane with random content.
    expectedRgbData = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 3);
    random.nextBytes(expectedRgbData.array());

    // Upload the RGB byte buffer data as a texture.
    rgbTexture = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, rgbTexture);
    GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGB, WIDTH, HEIGHT, 0, GLES20.GL_RGB,
        GLES20.GL_UNSIGNED_BYTE, expectedRgbData);
    GlUtil.checkNoGLES2Error("glTexImage2D");

    TextureBufferImpl buffer = new TextureBufferImpl(WIDTH, HEIGHT, TextureBuffer.Type.RGB,
        rgbTexture, new Matrix() /* transformMatrix */, null /* surfaceTextureHelper */,
        null /* releaseCallback */);
    rgbFrame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);

    drawer = new RectGlVideoFrameDrawer();
  }

  @After
  public void tearDow() {
    GLES20.glDeleteTextures(1, new int[] {rgbTexture}, 0);
    rgbFrame.release();

    drawer.dispose();
    eglBase.release();
  }

  private void checkScreenContent() {
    // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g. Nexus 9.
    final ByteBuffer actualRgbaData = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4);
    GLES20.glReadPixels(
        0, 0, WIDTH, HEIGHT, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, actualRgbaData);
    GlUtil.checkNoGLES2Error("glReadPixels");

    // Assert rendered image is pixel perfect to source RGB.
    assertByteBufferEquals(WIDTH, HEIGHT, stripAlphaChannel(actualRgbaData), expectedRgbData);
  }

  @Test
  @SmallTest
  public void testRgbRendering() {
    // Draw the RGB frame onto the pixel buffer.
    drawer.drawFrame(rgbFrame, new Matrix() /* additionalRenderMatrix */, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);
    checkScreenContent();
  }

  @Test
  @SmallTest
  public void testI420Rendering() {
    // TODO(sakal): Convert RGB texture to I420 once YuvConverter is ready.
    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);

    // Draw the RGB frame onto the pixel buffer.
    drawer.drawFrame(frame, new Matrix() /* additionalRenderMatrix */, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);
    frame.release();
  }

  /**
   * The purpose here is to test RectGlVideoFrameDrawer.drawFrame() with OES texture buffer.
   * Unfortunately, there is no easy way to create an OES texture. Most of the test is concerned
   * with creating OES textures in the following way:
   *  - Create SurfaceTexture with help from SurfaceTextureHelper.
   *  - Create an EglBase with the SurfaceTexture as EGLSurface.
   *  - Upload RGB texture with known content.
   *  - Draw the RGB texture onto the EglBase with the SurfaceTexture as target.
   *  - Wait for an OES texture to be produced.
   */
  @Test
  @MediumTest
  public void testOesRendering() throws InterruptedException {
    /**
     * Stub class to convert RGB ByteBuffers to OES textures by drawing onto a SurfaceTexture.
     */
    class StubOesTextureProducer {
      private final EglBase eglBase;
      private final RectGlVideoFrameDrawer drawer;

      public StubOesTextureProducer(EglBase.Context sharedContext, SurfaceTexture surfaceTexture) {
        eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PLAIN);
        surfaceTexture.setDefaultBufferSize(WIDTH, HEIGHT);
        eglBase.createSurface(surfaceTexture);
        assertEquals(eglBase.surfaceWidth(), WIDTH);
        assertEquals(eglBase.surfaceHeight(), HEIGHT);

        drawer = new RectGlVideoFrameDrawer();
        eglBase.makeCurrent();
      }

      public void draw() {
        eglBase.makeCurrent();

        // Draw the RGB data onto the SurfaceTexture.
        drawer.drawFrame(rgbFrame, new Matrix() /* additionalRenderMatrix */, 0 /* viewportX */,
            0 /* viewportY */, WIDTH, HEIGHT);
        eglBase.swapBuffers();
      }

      public void release() {
        eglBase.makeCurrent();
        drawer.dispose();
        eglBase.release();
      }
    }

    // Create resources for generating OES textures.
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, eglBase.getEglBaseContext());
    final StubOesTextureProducer oesProducer = new StubOesTextureProducer(
        eglBase.getEglBaseContext(), surfaceTextureHelper.getSurfaceTexture());
    final SurfaceTextureHelperTest.MockTextureListener listener =
        new SurfaceTextureHelperTest.MockTextureListener();
    surfaceTextureHelper.startListening(listener);

    // Draw the frame and block until an OES texture is delivered.
    oesProducer.draw();
    listener.waitForNewFrame();
    eglBase.makeCurrent(); // oesProducer.draw changed the eglBase, switch back to the original one

    // Real test starts here.
    final TextureBuffer buffer = surfaceTextureHelper.createTextureBuffer(WIDTH, HEIGHT,
        RendererCommon.convertMatrixToAndroidGraphicsMatrix(listener.transformMatrix));
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);

    // Draw the OES texture on the pixel buffer.
    drawer.drawFrame(frame, new Matrix() /* additionalRenderMatrix */, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);
    frame.release();

    checkScreenContent();

    oesProducer.release();
    surfaceTextureHelper.dispose();
  }

  // Assert RGB ByteBuffers are pixel perfect identical.
  private static void assertByteBufferEquals(
      int width, int height, ByteBuffer actual, ByteBuffer expected) {
    actual.rewind();
    expected.rewind();
    assertEquals(actual.remaining(), width * height * 3);
    assertEquals(expected.remaining(), width * height * 3);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        final int actualR = actual.get() & 0xFF;
        final int actualG = actual.get() & 0xFF;
        final int actualB = actual.get() & 0xFF;
        final int expectedR = expected.get() & 0xFF;
        final int expectedG = expected.get() & 0xFF;
        final int expectedB = expected.get() & 0xFF;
        if (actualR != expectedR || actualG != expectedG || actualB != expectedB) {
          fail("ByteBuffers of size " + width + "x" + height + " not equal at position "
              + "(" + x + ", " + y + "). Expected color (R,G,B): "
              + "(" + expectedR + ", " + expectedG + ", " + expectedB + ")"
              + " but was: "
              + "(" + actualR + ", " + actualG + ", " + actualB + ").");
        }
      }
    }
  }

  // Convert RGBA ByteBuffer to RGB ByteBuffer.
  private static ByteBuffer stripAlphaChannel(ByteBuffer rgbaBuffer) {
    rgbaBuffer.rewind();
    assertEquals(rgbaBuffer.remaining() % 4, 0);
    final int numberOfPixels = rgbaBuffer.remaining() / 4;
    final ByteBuffer rgbBuffer = ByteBuffer.allocateDirect(numberOfPixels * 3);
    while (rgbaBuffer.hasRemaining()) {
      // Copy RGB.
      for (int channel = 0; channel < 3; ++channel) {
        rgbBuffer.put(rgbaBuffer.get());
      }
      // Drop alpha.
      rgbaBuffer.get();
    }
    return rgbBuffer;
  }
}
