/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.media.MediaCodecInfo;
import android.support.test.filters.SmallTest;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public final class HardwareVideoEncoderInstrumentationTest {
  @ParameterAnnotations.ClassParameter
  private static final List<ParameterSet> CLASS_PARAMS = new ArrayList<>();

  private enum BufferType {
    I420,
    TEXTURE,
  }

  private static final List<String> CODEC_NAMES = Arrays.asList("VP8", "H264");
  private static final List<Boolean> USE_EGL_CONTEXT = Arrays.asList(false, true);
  private static final List<BufferType> BUFFER_TYPES =
      Arrays.asList(BufferType.I420, BufferType.TEXTURE);
  private static final List<Integer> INPUT_FRAME_MULTIPLIERS = Arrays.asList(1, 2);
  private static final List<Integer> ENCODER_YUV_FORMATS = Arrays.asList(
      // HardwareVideoEncoder.YuvFormat.I420
      MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar,
      // HardwareVideoEncoder.YuvFormat.NV12
      MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar);

  static {
    for (String codecName : CODEC_NAMES) {
      for (Boolean useEglContext : USE_EGL_CONTEXT) {
        for (BufferType bufferType : BUFFER_TYPES) {
          for (Integer inputFrameMultiplier : INPUT_FRAME_MULTIPLIERS) {
            for (Integer encoderYuvFormat : ENCODER_YUV_FORMATS) {
              CLASS_PARAMS.add(new ParameterSet()
                                   .value(codecName, useEglContext, bufferType.name(),
                                       inputFrameMultiplier, encoderYuvFormat)
                                   .name(codecName + (useEglContext ? "With" : "Without")
                                       + "EglContext_" + bufferType.name() + "x"
                                       + inputFrameMultiplier + "_YuvFormat" + encoderYuvFormat));
            }
          }
        }
      }
    }
  }

  private final VideoCodecInfo codecType;
  private final boolean useEglContext;
  private final BufferType bufferType;
  private final Integer inputFrameMultiplier;
  private final Integer encoderYuvFormat;

  public HardwareVideoEncoderInstrumentationTest(String codecName, boolean useEglContext,
      String bufferTypeName, Integer inputFrameMultiplier, Integer encoderYuvFormat) {
    if (codecName.equals("H264")) {
      this.codecType = H264Utils.DEFAULT_H264_BASELINE_PROFILE_CODEC;
    } else {
      this.codecType = new VideoCodecInfo(codecName, new HashMap<>());
    }
    this.useEglContext = useEglContext;
    this.bufferType = BufferType.valueOf(bufferTypeName);
    this.inputFrameMultiplier = inputFrameMultiplier;
    this.encoderYuvFormat = encoderYuvFormat;
  }

  private static final int TEST_FRAME_COUNT = 10;
  private static final int TEST_FRAME_WIDTH = 640;
  private static final int TEST_FRAME_HEIGHT = 360;
  private VideoFrame.Buffer[] TEST_FRAMES;

  private static final boolean ENABLE_INTEL_VP8_ENCODER = true;
  private static final boolean ENABLE_H264_HIGH_PROFILE = true;
  private static final VideoEncoder.Settings ENCODER_SETTINGS = new VideoEncoder.Settings(
      1 /* core */,
      getAlignedNumber(TEST_FRAME_WIDTH, HardwareVideoEncoderTest.getPixelAlignmentRequired()),
      getAlignedNumber(TEST_FRAME_HEIGHT, HardwareVideoEncoderTest.getPixelAlignmentRequired()),
      300 /* kbps */, 30 /* fps */, 1 /* numberOfSimulcastStreams */, true /* automaticResizeOn */,
      /* capabilities= */ new VideoEncoder.Capabilities(false /* lossNotification */));

  private static final int DECODE_TIMEOUT_MS = 1000;
  private static final VideoDecoder.Settings SETTINGS = new VideoDecoder.Settings(1 /* core */,
      getAlignedNumber(TEST_FRAME_WIDTH, HardwareVideoEncoderTest.getPixelAlignmentRequired()),
      getAlignedNumber(TEST_FRAME_HEIGHT, HardwareVideoEncoderTest.getPixelAlignmentRequired()));

  private static class MockDecodeCallback implements VideoDecoder.Callback {
    private final BlockingQueue<VideoFrame> frameQueue = new LinkedBlockingQueue<>();

    @Override
    public void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp) {
      assertNotNull(frame);
      frameQueue.offer(frame);
    }

    public void assertFrameDecoded(VideoFrame.Buffer inputBuffer, int inputFrameMultiplier) {
      VideoFrame decodedFrame = poll();
      VideoFrame.Buffer decodedBuffer = decodedFrame.getBuffer();
      double error = 0.03;
      double delta = Math.max(inputBuffer.getWidth(), inputBuffer.getHeight()) * error;
      assertEquals((double) inputBuffer.getWidth() / (double) inputFrameMultiplier,
          (double) decodedBuffer.getWidth(), delta);
      assertEquals((double) inputBuffer.getHeight() / (double) inputFrameMultiplier,
          (double) decodedBuffer.getHeight(), delta);
    }

    public VideoFrame poll() {
      try {
        VideoFrame frame = frameQueue.poll(DECODE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertNotNull("Timed out waiting for the frame to be decoded.", frame);
        return frame;
      } catch (InterruptedException e) {
        throw new RuntimeException(e);
      }
    }
  }

  private static VideoFrame.Buffer[] generateTestFrames(
      EglBase.Context eglContext, BufferType bufferType, Integer inputFrameMultiplier) {
    VideoFrame.Buffer[] result = new VideoFrame.Buffer[TEST_FRAME_COUNT];
    for (int i = 0; i < TEST_FRAME_COUNT; i++) {
      VideoFrame.I420Buffer i420Buffer =
          JavaI420Buffer.allocate(getAlignedNumber(TEST_FRAME_WIDTH * inputFrameMultiplier,
                                      HardwareVideoEncoderTest.getPixelAlignmentRequired()),
              getAlignedNumber(TEST_FRAME_HEIGHT * inputFrameMultiplier,
                  HardwareVideoEncoderTest.getPixelAlignmentRequired()));
      switch (bufferType) {
        case I420:
          result[i] = i420Buffer;
          break;
        case TEXTURE:
          result[i] = VideoFrameBufferTest.createOesTextureBuffer(eglContext, i420Buffer);
          break;
      }
    }
    return result;
  }

  private final EncodedImage[] encodedTestFrames = new EncodedImage[TEST_FRAME_COUNT];
  private EglBase14 eglBase;

  private VideoDecoderFactory createDecoderFactory(EglBase.Context eglContext) {
    return new HardwareVideoDecoderFactory(eglContext);
  }

  private @Nullable VideoDecoder createDecoder() {
    VideoDecoderFactory factory =
        createDecoderFactory(useEglContext ? eglBase.getEglBaseContext() : null);
    return factory.createDecoder(codecType);
  }

  private void encodeTestFrames() {
    HardwareVideoEncoderFactory encoderFactory = new HardwareVideoEncoderFactory(
        eglBase.getEglBaseContext(), ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
    VideoEncoder encoder = encoderFactory.createEncoderForTesting(codecType, encoderYuvFormat);
    assertNotNull(encoder);
    HardwareVideoEncoderTest.MockEncoderCallback encodeCallback =
        new HardwareVideoEncoderTest.MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(ENCODER_SETTINGS, encodeCallback));

    long lastTimestampNs = 0;
    for (int i = 0; i < TEST_FRAME_COUNT; i++) {
      lastTimestampNs += TimeUnit.SECONDS.toNanos(1) / ENCODER_SETTINGS.maxFramerate;
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});
      HardwareVideoEncoderTest.testEncodeFrame(
          encoder, new VideoFrame(TEST_FRAMES[i], 0 /* rotation */, lastTimestampNs), info);
      encodedTestFrames[i] = encodeCallback.poll();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  private static int getAlignedNumber(int number, int alignment) {
    return (number / alignment) * alignment;
  }

  @Before
  public void setUp() {
    NativeLibrary.initialize(new NativeLibrary.DefaultLoader(), TestConstants.NATIVE_LIBRARY);

    eglBase = EglBase.createEgl14(EglBase.CONFIG_PLAIN);
    eglBase.createDummyPbufferSurface();
    eglBase.makeCurrent();

    TEST_FRAMES = generateTestFrames(eglBase.getEglBaseContext(), bufferType, inputFrameMultiplier);

    encodeTestFrames();
  }

  @After
  public void tearDown() {
    for (VideoFrame.Buffer buffer : TEST_FRAMES) {
      buffer.release();
    }
    eglBase.release();
  }

  @Test
  @SmallTest
  public void testDecode() {
    VideoDecoder decoder = createDecoder();
    assertNotNull(decoder);
    MockDecodeCallback callback = new MockDecodeCallback();
    assertEquals(VideoCodecStatus.OK, decoder.initDecode(SETTINGS, callback));

    for (int i = 0; i < TEST_FRAME_COUNT; i++) {
      assertEquals(VideoCodecStatus.OK,
          decoder.decode(encodedTestFrames[i],
              new VideoDecoder.DecodeInfo(false /* isMissingFrames */, 0 /* renderTimeMs */)));
      callback.assertFrameDecoded(TEST_FRAMES[i], inputFrameMultiplier);
    }

    assertEquals(VideoCodecStatus.OK, decoder.release());
  }
}
