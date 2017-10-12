/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.same;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.webrtc.CameraEnumerationAndroid.getClosestSupportedFramerateRange;

import android.graphics.Matrix;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.webrtc.GenericGlVideoFrameDrawer.FrameFormat;
import org.webrtc.GenericGlVideoFrameDrawer.FrameInformation;
import org.webrtc.GenericGlVideoFrameDrawer.ShaderDefinition;
import org.webrtc.VideoFrame.I420Buffer;
import org.webrtc.VideoFrame.TextureBuffer;

/**
 * Tests for GericGlVideoFrameDrawer.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMatrix.class})
public class GenericGlVideoFrameDrawerTest {
  private static int WIDTH = 16;
  private static int HEIGHT = 32;

  GenericGlVideoFrameDrawer drawer;
  ShaderDefinition shaderDefinition;
  GlShader shader;

  @Before
  public void setUp() {
    // Log to stdout.
    ShadowLog.stream = System.out;

    shaderDefinition = mock(ShaderDefinition.class);
    shader = mock(GlShader.class);
    drawer = new GenericGlVideoFrameDrawer(shaderDefinition);

    when(shaderDefinition.getShader(any(FrameFormat.class))).thenReturn(shader);
  }

  @Test
  public void testI420() {
    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(frame, null /* additionalRenderMatrix */, 0 /* viewportX */, 0 /* viewportY */,
        WIDTH, HEIGHT);
    frame.release();

    // Check that correct frame format is passed.
    ArgumentCaptor<FrameFormat> frameFormatCaptor = ArgumentCaptor.forClass(FrameFormat.class);
    verify(shaderDefinition).getShader(frameFormatCaptor.capture());
    FrameFormat frameFormat = frameFormatCaptor.getValue();
    assertNotNull(frameFormat);
    assertTrue(frameFormat.isYuv());
    assertNull(frameFormat.getTextureType());

    verify(shaderDefinition).prepareShader(frameFormat, shader);

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(same(frameFormat), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(WIDTH, frameInfo.renderWidth);
    assertEquals(HEIGHT, frameInfo.renderHeight);

    verifyNoMoreInteractions(shaderDefinition);
  }

  private void testTextureType(TextureBuffer.Type textureType) {
    final TextureBuffer buffer = mock(TextureBuffer.class);
    when(buffer.getType()).thenReturn(textureType);
    when(buffer.getTransformMatrix()).thenReturn(new Matrix());
    when(buffer.getWidth()).thenReturn(WIDTH);
    when(buffer.getHeight()).thenReturn(HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(frame, null /* additionalRenderMatrix */, 0 /* viewportX */, 0 /* viewportY */,
        WIDTH, HEIGHT);
    frame.release();

    // Check that correct frame format is passed.
    ArgumentCaptor<FrameFormat> frameFormatCaptor = ArgumentCaptor.forClass(FrameFormat.class);
    verify(shaderDefinition).getShader(frameFormatCaptor.capture());
    FrameFormat frameFormat = frameFormatCaptor.getValue();
    assertNotNull(frameFormat);
    assertFalse(frameFormat.isYuv());
    assertEquals(textureType, frameFormat.getTextureType());

    verify(shaderDefinition).prepareShader(frameFormat, shader);

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(same(frameFormat), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(WIDTH, frameInfo.renderWidth);
    assertEquals(HEIGHT, frameInfo.renderHeight);

    verifyNoMoreInteractions(shaderDefinition);
  }

  @Test
  public void testRgb() {
    testTextureType(TextureBuffer.Type.RGB);
  }

  @Test
  public void testOes() {
    testTextureType(TextureBuffer.Type.OES);
  }

  @Test
  public void testChangeFormat() {
    final InOrder inOrder = inOrder(shaderDefinition);

    // === Feed in I420 frame.
    final I420Buffer i420Buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame i420Frame = new VideoFrame(i420Buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(i420Frame, null /* additionalRenderMatrix */, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);
    i420Frame.release();

    // Check that correct frame format is passed.
    ArgumentCaptor<FrameFormat> frameFormatCaptor = ArgumentCaptor.forClass(FrameFormat.class);
    verify(shaderDefinition).getShader(frameFormatCaptor.capture());
    FrameFormat frameFormat = frameFormatCaptor.getValue();
    assertNotNull(frameFormat);
    assertTrue(frameFormat.isYuv());
    assertNull(frameFormat.getTextureType());

    inOrder.verify(shaderDefinition).prepareShader(frameFormat, shader);

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    inOrder.verify(shaderDefinition)
        .updateShader(same(frameFormat), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(WIDTH, frameInfo.renderWidth);
    assertEquals(HEIGHT, frameInfo.renderHeight);

    inOrder.verifyNoMoreInteractions();

    // === Feed in OES frame.
    final TextureBuffer textureBuffer = mock(TextureBuffer.class);
    when(textureBuffer.getType()).thenReturn(TextureBuffer.Type.OES);
    when(textureBuffer.getTransformMatrix()).thenReturn(new Matrix());
    when(textureBuffer.getWidth()).thenReturn(WIDTH);
    when(textureBuffer.getHeight()).thenReturn(HEIGHT);
    final VideoFrame textureFrame =
        new VideoFrame(textureBuffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(textureFrame, null /* additionalRenderMatrix */, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);

    // Check that correct frame format is passed.
    inOrder.verify(shaderDefinition).getShader(frameFormatCaptor.capture());
    frameFormat = frameFormatCaptor.getValue();
    assertNotNull(frameFormat);
    assertFalse(frameFormat.isYuv());
    assertEquals(TextureBuffer.Type.OES, frameFormat.getTextureType());

    inOrder.verify(shaderDefinition).prepareShader(frameFormat, shader);

    // Check that updateShader is called with correct frameInfo.
    inOrder.verify(shaderDefinition)
        .updateShader(same(frameFormat), same(shader), frameInfoCaptor.capture());
    frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(WIDTH, frameInfo.renderWidth);
    assertEquals(HEIGHT, frameInfo.renderHeight);

    inOrder.verifyNoMoreInteractions();

    // === Feed in RGB frame.
    when(textureBuffer.getType()).thenReturn(TextureBuffer.Type.RGB); // Change texture type.
    drawer.drawFrame(textureFrame, null /* additionalRenderMatrix */, 0 /* viewportX */,
        0 /* viewportY */, WIDTH, HEIGHT);
    textureFrame.release();

    // Check that correct frame format is passed.
    inOrder.verify(shaderDefinition).getShader(frameFormatCaptor.capture());
    frameFormat = frameFormatCaptor.getValue();
    assertNotNull(frameFormat);
    assertFalse(frameFormat.isYuv());
    assertEquals(TextureBuffer.Type.RGB, frameFormat.getTextureType());

    inOrder.verify(shaderDefinition).prepareShader(frameFormat, shader);

    // Check that updateShader is called with correct frameInfo.
    inOrder.verify(shaderDefinition)
        .updateShader(same(frameFormat), same(shader), frameInfoCaptor.capture());
    frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(WIDTH, frameInfo.renderWidth);
    assertEquals(HEIGHT, frameInfo.renderHeight);

    inOrder.verifyNoMoreInteractions();
  }

  @Test
  public void testUpdateShaderCalledOncePerFrame() {
    final InOrder inOrder = inOrder(shaderDefinition);

    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);

    drawer.drawFrame(frame, null /* additionalRenderMatrix */, 0 /* viewportX */, 0 /* viewportY */,
        WIDTH, HEIGHT);
    inOrder.verify(shaderDefinition).getShader(any(FrameFormat.class));
    inOrder.verify(shaderDefinition).prepareShader(any(FrameFormat.class), same(shader));

    for (int i = 1; i <= 10; i++) {
      if (i != 1) { // The frame was already drawn above for the first loop.
        // Draw the next frame.
        drawer.drawFrame(frame, null /* additionalRenderMatrix */, 0 /* viewportX */,
            0 /* viewportY */, WIDTH, HEIGHT);
      }

      // Check that updateShader is called with correct frameInfo.
      ArgumentCaptor<FrameInformation> frameInfoCaptor =
          ArgumentCaptor.forClass(FrameInformation.class);
      inOrder.verify(shaderDefinition)
          .updateShader(any(FrameFormat.class), same(shader), frameInfoCaptor.capture());
      FrameInformation frameInfo = frameInfoCaptor.getValue();
      assertNotNull(frameInfo);
      assertEquals(WIDTH, frameInfo.renderWidth);
      assertEquals(HEIGHT, frameInfo.renderHeight);
    }

    frame.release();

    inOrder.verifyNoMoreInteractions();
  }

  @Test
  public void testAdditionalRenderMatrixAffectsRenderSize() {
    final float scaleX = 0.5f;
    final float scaleY = 0.25f;

    final Matrix additionalRenderMatrix = new Matrix();
    additionalRenderMatrix.preScale(scaleX, scaleY);

    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(
        frame, additionalRenderMatrix, 0 /* viewportX */, 0 /* viewportY */, WIDTH, HEIGHT);
    frame.release();

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(any(FrameFormat.class), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals((int) (scaleX * WIDTH), frameInfo.renderWidth);
    assertEquals((int) (scaleY * HEIGHT), frameInfo.renderHeight);
  }

  @Test
  public void testAdditionalRenderMatrixCanBeRotated() {
    final float scaleX = 0.5f;
    final float scaleY = 0.25f;

    final Matrix additionalRenderMatrix = new Matrix();
    additionalRenderMatrix.preScale(scaleX, scaleY);
    additionalRenderMatrix.preRotate(20f /* degrees */);

    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(
        frame, additionalRenderMatrix, 0 /* viewportX */, 0 /* viewportY */, WIDTH, HEIGHT);
    frame.release();

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(any(FrameFormat.class), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals((int) (scaleX * WIDTH), frameInfo.renderWidth);
    assertEquals((int) (scaleY * HEIGHT), frameInfo.renderHeight);
  }

  @Test
  public void testAdditionalRenderMatrixCanBeTranslated() {
    final float scaleX = 0.5f;
    final float scaleY = 0.25f;

    final Matrix additionalRenderMatrix = new Matrix();
    additionalRenderMatrix.preScale(scaleX, scaleY);
    additionalRenderMatrix.preTranslate(3f /* x */, 4f /* y */);

    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(
        frame, additionalRenderMatrix, 0 /* viewportX */, 0 /* viewportY */, WIDTH, HEIGHT);
    frame.release();

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(any(FrameFormat.class), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals((int) (scaleX * WIDTH), frameInfo.renderWidth);
    assertEquals((int) (scaleY * HEIGHT), frameInfo.renderHeight);
  }

  @Test
  public void testFrameRotationAffectRenderSize() {
    final I420Buffer buffer = JavaI420Buffer.allocate(WIDTH, HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 90 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(frame, null /* additionalRenderMatrix */, 0 /* viewportX */, 0 /* viewportY */,
        WIDTH, HEIGHT);
    frame.release();

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(any(FrameFormat.class), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(HEIGHT, frameInfo.renderWidth);
    assertEquals(WIDTH, frameInfo.renderHeight);
  }

  @Test
  public void testTextureTransformationMatrixIsApplied() {
    final float scaleX = 0.5f;
    final float scaleY = 0.25f;

    final Matrix transformationMatrix = new Matrix();
    transformationMatrix.preScale(scaleX, scaleY);

    final TextureBuffer buffer = mock(TextureBuffer.class);
    when(buffer.getType()).thenReturn(TextureBuffer.Type.OES);
    when(buffer.getTransformMatrix()).thenReturn(new Matrix(transformationMatrix));
    when(buffer.getWidth()).thenReturn(WIDTH);
    when(buffer.getHeight()).thenReturn(HEIGHT);
    final VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, 0 /* timestampNs */);
    drawer.drawFrame(frame, null /* additionalRenderMatrix */, 0 /* viewportX */, 0 /* viewportY */,
        WIDTH, HEIGHT);
    frame.release();

    // Check that updateShader is called with correct frameInfo.
    ArgumentCaptor<FrameInformation> frameInfoCaptor =
        ArgumentCaptor.forClass(FrameInformation.class);
    verify(shaderDefinition)
        .updateShader(any(FrameFormat.class), same(shader), frameInfoCaptor.capture());
    FrameInformation frameInfo = frameInfoCaptor.getValue();
    assertNotNull(frameInfo);
    assertEquals(transformationMatrix, frameInfo.renderMatrix);
    assertEquals(WIDTH, frameInfo.renderWidth);
    assertEquals(HEIGHT, frameInfo.renderHeight);
  }
}
