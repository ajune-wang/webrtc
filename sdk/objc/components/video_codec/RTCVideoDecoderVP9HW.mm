/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoDecoderVP9HW.h"

#if defined(RTC_ENABLE_VP9)

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts) : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

@interface RTC_OBJC_TYPE (RTCVideoDecoderVP9HW)
() - (void)setError : (OSStatus)error;
@end

absl::optional<rtc::ScopedCFTypeRef<CMSampleBufferRef>> VP9BufferToCMSampleBuffer(
    const uint8_t *buffer, size_t buffer_size, CMVideoFormatDescriptionRef video_format) {
  rtc::ScopedCFTypeRef<CMBlockBufferRef> new_block_buffer;
  if (auto error = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
                                                      NULL,
                                                      buffer_size,
                                                      kCFAllocatorDefault,
                                                      NULL,
                                                      0,
                                                      buffer_size,
                                                      kCMBlockBufferAssureMemoryNowFlag,
                                                      new_block_buffer.InitializeInto())) {
    RTC_LOG(LS_ERROR) << "CMBlockBufferCreateWithMemoryBlock failed with: " << error;
    return absl::nullopt;
  }

  if (auto error = CMBlockBufferReplaceDataBytes(buffer, new_block_buffer.get(), 0, buffer_size)) {
    RTC_LOG(LS_ERROR) << "CMBlockBufferReplaceDataBytes failed with: " << error;
    return absl::nullopt;
  }

  rtc::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  if (auto error = CMSampleBufferCreate(kCFAllocatorDefault,
                                        new_block_buffer.get(),
                                        true,
                                        nullptr,
                                        nullptr,
                                        video_format,
                                        1,
                                        0,
                                        nullptr,
                                        0,
                                        nullptr,
                                        sample_buffer.InitializeInto())) {
    RTC_LOG(LS_ERROR) << "CMSampleBufferCreate failed with: " << error;
    return absl::nullopt;
  }
  return sample_buffer;
}

// This is the callback function that VideoToolbox calls when decode is
// complete.
void vp9DecompressionOutputCallback(void *decoderRef,
                                    void *params,
                                    OSStatus status,
                                    VTDecodeInfoFlags infoFlags,
                                    CVImageBufferRef imageBuffer,
                                    CMTime timestamp,
                                    CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(
      reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr) {
    RTC_OBJC_TYPE(RTCVideoDecoderVP9HW) *decoder =
        (__bridge RTC_OBJC_TYPE(RTCVideoDecoderVP9HW) *)decoderRef;
    [decoder setError:status];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }
  RTC_OBJC_TYPE(RTCCVPixelBuffer) *frameBuffer =
      [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc] initWithPixelBuffer:imageBuffer];
  RTC_OBJC_TYPE(RTCVideoFrame) *decodedFrame = [[RTC_OBJC_TYPE(RTCVideoFrame) alloc]
      initWithBuffer:frameBuffer
            rotation:RTCVideoRotation_0
         timeStampNs:CMTimeGetSeconds(timestamp) * NSEC_PER_SEC];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

// Decoder.
@implementation RTC_OBJC_TYPE (RTCVideoDecoderVP9HW) {
  CMVideoFormatDescriptionRef _videoFormat;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
}

- (void)dealloc {
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);

  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat;
  CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                 'vp09',
                                 inputImage.encodedWidth,
                                 inputImage.encodedHeight,
                                 nullptr,
                                 inputFormat.InitializeInto());
  if (inputFormat) {
    // Check if the video format has changed, and reinitialize decoder if
    // needed.
    if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
      [self setVideoFormat:inputFormat.get()];
      int resetDecompressionSessionError = [self resetDecompressionSession];
      if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
        return resetDecompressionSessionError;
      }
    }
  }
  if (!_videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it. This can happen after backgrounding.
    RTC_LOG(LS_WARNING) << "Missing video format.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  auto sampleBuffer = VP9BufferToCMSampleBuffer(
      (uint8_t *)inputImage.buffer.bytes, inputImage.buffer.length, _videoFormat);
  if (!sampleBuffer) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  auto frameDecodeParams = absl::make_unique<RTCFrameDecodeParams>(_callback, inputImage.timeStamp);
  OSStatus status = VTDecompressionSessionDecodeFrame(_decompressionSession,
                                                      sampleBuffer->get(),
                                                      decodeFlags,
                                                      frameDecodeParams.release(),
                                                      nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active or decoder malfunctions and retry the decode request.
  if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_INFO) << "Failed to decode frame with code: " << status
                     << " retrying decode after decompression session reset";
    frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp));
    status = VTDecompressionSessionDecodeFrame(_decompressionSession,
                                               sampleBuffer->get(),
                                               decodeFlags,
                                               frameDecodeParams.release(),
                                               nullptr);
  }
#endif
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (void)setError:(OSStatus)error {
  _error = error;
}

- (NSInteger)releaseDecoder {
  // Need to invalidate the session so that callbacks no longer occur and it
  // is safe to null out the callback.
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
  [self destroyDecompressionSession];

  // Need to wait for the first SPS to initialize decoder.
  if (!_videoFormat) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  // TODO(tkchin): Maybe only set OpenGL/IOSurface keys if we know that that
  // we can pass CVPixelBuffers as native handles in decoder output.
#if TARGET_OS_SIMULATOR
  static size_t const attributesSize = 2;
#else
  static size_t const attributesSize = 3;
#endif

  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_IOS)
      kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
      kCVPixelBufferOpenGLCompatibilityKey,
#endif
#if !(TARGET_OS_SIMULATOR)
      kCVPixelBufferIOSurfacePropertiesKey,
#endif
      kCVPixelBufferPixelFormatTypeKey};
  CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
#if TARGET_OS_SIMULATOR
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, pixelFormat};
#else
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue, pixelFormat};
#endif

  CFDictionaryRef attributes = CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }
  VTDecompressionOutputCallbackRecord record = {
      vp9DecompressionOutputCallback,
      (__bridge void *)self,
  };
  OSStatus status = VTDecompressionSessionCreate(
      nullptr, _videoFormat, nullptr, attributes, &record, &_decompressionSession);
  CFRelease(attributes);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create decompression session: " << status;
    [self destroyDecompressionSession];
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureDecompressionSession];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
  RTC_DCHECK(_decompressionSession);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(_decompressionSession, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
  if (_decompressionSession) {
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat {
  if (_videoFormat == videoFormat) {
    return;
  }
  if (_videoFormat) {
    CFRelease(_videoFormat);
  }
  _videoFormat = videoFormat;
  if (_videoFormat) {
    CFRetain(_videoFormat);
  }
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

@end

#endif
