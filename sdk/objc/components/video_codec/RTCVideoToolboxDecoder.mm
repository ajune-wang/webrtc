/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoToolboxDecoder.h"

#import "RTCVideoToolboxDecoder+Private.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#include "frame_decode_params.h"
#import "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

// This is the callback function that VideoToolbox calls when decode is
// complete.
void decompressionOutputCallback(void *decoderRef,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration) {
  std::unique_ptr<FrameDecodeParams> decodeParams(reinterpret_cast<FrameDecodeParams *>(params));
  if (status != noErr) {
    RTC_OBJC_TYPE(RTCVideoToolboxDecoder) *decoder =
        (__bridge RTC_OBJC_TYPE(RTCVideoToolboxDecoder) *)decoderRef;
    [decoder setError:status];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }

  RTC_OBJC_TYPE(RTCCVPixelBuffer) *frameBuffer =
      [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc] initWithPixelBuffer:imageBuffer];
  RTC_OBJC_TYPE(RTCVideoFrame) *decodedFrame = [[RTC_OBJC_TYPE(RTCVideoFrame) alloc]
      initWithBuffer:frameBuffer
            rotation:RTCVideoRotation_0
         timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

@implementation RTC_OBJC_TYPE (RTCVideoToolboxDecoder)

@synthesize videoFormat = _videoFormat;
@synthesize decompressionSession = _decompressionSession;
@synthesize callback = _callback;
@synthesize error = _error;

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)dealloc {
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)releaseDecoder {
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (void)setError:(OSStatus)error {
  _error = error;
}

- (int)resetDecompressionSession:(NSDictionary *)decoderConfig
                       attribute:(NSDictionary *)attributes {
  [self destroyDecompressionSession];

  if (!_videoFormat) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  VTDecompressionOutputCallbackRecord record = {
      decompressionOutputCallback,
      (__bridge void *)self,
  };
  OSStatus status = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                                 _videoFormat,
                                                 (__bridge CFDictionaryRef)decoderConfig,
                                                 (__bridge CFDictionaryRef)attributes,
                                                 &record,
                                                 &_decompressionSession);
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
#if defined(WEBRTC_IOS)
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK_NOTREACHED() << "Virtual method not implemented in subclass.";
  return WEBRTC_VIDEO_CODEC_ERROR;
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
