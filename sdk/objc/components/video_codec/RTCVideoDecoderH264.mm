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

#import "RTCVideoDecoderH264.h"

#import <VideoToolbox/VideoToolbox.h>

#import "RTCVideoToolboxDecoder+Private.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "frame_decode_params.h"
#import "helpers/scoped_cftyperef.h"

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

// Decoder.
@implementation RTC_OBJC_TYPE (RTCVideoDecoderH264) {
  CMMemoryPoolRef _memoryPool;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _memoryPool = CMMemoryPoolCreate(nil);
  }
  return self;
}

- (void)dealloc {
  CMMemoryPoolInvalidate(_memoryPool);
  CFRelease(_memoryPool);
}

// TODO(bugs.webrtc.org/15444): Remove obsolete missingFrames param.
- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);

  if (self.error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    self.error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat =
      rtc::ScopedCF(webrtc::CreateVideoFormatDescription(
          reinterpret_cast<const uint8_t *>(inputImage.buffer.bytes), inputImage.buffer.length));
  if (inputFormat) {
    // Check if the video format has changed, and reinitialize decoder if
    // needed.
    if (!CMFormatDescriptionEqual(inputFormat.get(), self.videoFormat)) {
      [self setVideoFormat:inputFormat.get()];
      int resetDecompressionSessionError = [self resetDecompressionSession];
      if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
        return resetDecompressionSessionError;
      }
    }
  }
  if (!self.videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    // This can happen after backgrounding. We need to wait for the next
    // sps/pps before we can resume so we request a keyframe by returning an
    // error.
    RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  CMSampleBufferRef sampleBuffer = nullptr;
  if (!webrtc::H264AnnexBBufferToCMSampleBuffer(
          reinterpret_cast<const uint8_t *>(inputImage.buffer.bytes),
          inputImage.buffer.length,
          self.videoFormat,
          &sampleBuffer,
          _memoryPool)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<FrameDecodeParams> frameDecodeParams;
  frameDecodeParams.reset(new FrameDecodeParams(inputImage, self.callback, inputImage.timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      self.decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active or decoder malfunctions and retry the decode request.
  if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_INFO) << "Failed to decode frame with code: " << status
                     << " retrying decode after decompression session reset";
    frameDecodeParams.reset(new FrameDecodeParams(inputImage, self.callback, inputImage.timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        self.decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
  }
#endif
  CFRelease(sampleBuffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  // TODO(tkchin): Maybe only set OpenGL/IOSurface keys if we know that that
  // we can pass CVPixelBuffers as native handles in decoder output.
  NSDictionary *attributes = @{
#if defined(WEBRTC_IOS) && (TARGET_OS_MACCATALYST || TARGET_OS_SIMULATOR)
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @(YES),
#elif defined(WEBRTC_IOS)
    (NSString *)kCVPixelBufferOpenGLESCompatibilityKey : @(YES),
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_ARCH_ARM64)
    (NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
#if !(TARGET_OS_SIMULATOR)
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
#endif
    (NSString *)
    kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange),
  };

  return [super resetDecompressionSession:nullptr attribute:attributes];
}



@end
