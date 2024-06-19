/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoVTDecoderAV1.h"

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
#include "third_party/libgav1/src/src/obu_parser.h"

CFStringRef GetPrimaries(const libgav1::ColorPrimary &primary_id) {
  switch (primary_id) {
    case libgav1::kColorPrimaryBt709:
    case libgav1::kColorPrimaryUnspecified:  // Assume BT.709.
      return kCMFormatDescriptionColorPrimaries_ITU_R_709_2;

    case libgav1::kColorPrimaryBt2020:
      return kCMFormatDescriptionColorPrimaries_ITU_R_2020;

    case libgav1::kColorPrimaryBt601:
    case libgav1::kColorPrimarySmpte240:
      return kCMFormatDescriptionColorPrimaries_SMPTE_C;

    case libgav1::kColorPrimaryBt470Bg:
      return kCMFormatDescriptionColorPrimaries_EBU_3213;

    case libgav1::kColorPrimarySmpte432:
      return kCMFormatDescriptionColorPrimaries_DCI_P3;

    case libgav1::kColorPrimarySmpte431:
      return kCMFormatDescriptionColorPrimaries_P3_D65;

    default:
      RTC_LOG(LS_ERROR) << "Unsupported primary id: " << static_cast<uint32_t>(primary_id);
      return nil;
  }
}

CFStringRef GetTransferFunction(const libgav1::TransferCharacteristics &transfer_id) {
  switch (transfer_id) {
    case libgav1::kTransferCharacteristicsLinear:
      return kCMFormatDescriptionTransferFunction_Linear;

    case libgav1::kTransferCharacteristicsSrgb:
      return kCVImageBufferTransferFunction_sRGB;

    case libgav1::kTransferCharacteristicsBt601:
    case libgav1::kTransferCharacteristicsBt709:
    case libgav1::kTransferCharacteristicsUnspecified:  // Assume BT.709.
      return kCMFormatDescriptionTransferFunction_ITU_R_709_2;

    case libgav1::kTransferCharacteristicsBt2020TenBit:
    case libgav1::kTransferCharacteristicsBt2020TwelveBit:
      return kCMFormatDescriptionTransferFunction_ITU_R_2020;

    case libgav1::kTransferCharacteristicsSmpte2084:
      return kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;

    case libgav1::kTransferCharacteristicsHlg:
      return kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;

    case libgav1::kTransferCharacteristicsSmpte240:
      return kCMFormatDescriptionTransferFunction_SMPTE_240M_1995;

    case libgav1::kTransferCharacteristicsSmpte428:
      return kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;

    default:
      RTC_LOG(LS_ERROR) << "Unsupported transfer function: " << static_cast<uint32_t>(transfer_id);
      return nil;
  }
}

CFStringRef GetMatrix(const libgav1::MatrixCoefficients &matrix_id) {
  switch (matrix_id) {
    case libgav1::kMatrixCoefficientsBt709:
    case libgav1::kMatrixCoefficientsUnspecified:  // Assume BT.709.
      return kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2;

    case libgav1::kMatrixCoefficientsBt2020Ncl:
      return kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;

    case libgav1::kMatrixCoefficientsFcc:
    case libgav1::kMatrixCoefficientsBt601:
    case libgav1::kMatrixCoefficientsBt470BG:
      // The FCC-based coefficients don't exactly match BT.601, but they're
      // close enough.
      return kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4;

    case libgav1::kMatrixCoefficientsSmpte240:
      return kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995;

    default:
      RTC_LOG(LS_ERROR) << "Unsupported matrix id: " << static_cast<uint32_t>(matrix_id);
      return nil;
  }
}

NSDictionary<NSString *, NSObject *> *createExtension(
    const libgav1::ObuSequenceHeader &sequence_header,
    const uint8_t *av1c,
    const size_t &av1c_size) {
  return @{
    (NSString *)kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms : @{
      @"av1C" : [NSData dataWithBytes:av1c length:av1c_size],
    },
    (NSString *)kCMFormatDescriptionExtension_FormatName : @"av01",
    // YCbCr without alpha uses 24. See
    // http://developer.apple.com/qa/qa2001/qa1183.html
    (NSString *)kCMFormatDescriptionExtension_Depth : @(24),
    @"BitsPerComponent" : @(sequence_header.color_config.bitdepth),
    (NSString *)kCMFormatDescriptionExtension_ColorPrimaries :
        (__bridge NSString *)GetPrimaries(sequence_header.color_config.color_primary),
    (NSString *)
    kCMFormatDescriptionExtension_TransferFunction : (__bridge NSString *)GetTransferFunction(
        sequence_header.color_config.transfer_characteristics),
    (NSString *)kCMFormatDescriptionExtension_YCbCrMatrix :
        (__bridge NSString *)GetMatrix(sequence_header.color_config.matrix_coefficients),
    (NSString *)kCMFormatDescriptionExtension_FullRangeVideo :
        @(sequence_header.color_config.color_range == libgav1::kColorRangeFull),
  };
}

bool BufferToCMSampleBuffer(const uint8_t *data,
                            const size_t &buffer_size,
                            CMVideoFormatDescriptionRef video_format,
                            CMSampleBufferRef *out_sample_buffer) {
  RTC_DCHECK(out_sample_buffer);
  RTC_DCHECK(video_format);
  *out_sample_buffer = nullptr;

  CMBlockBufferRef block_buffer = nullptr;
  OSStatus status =
      CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
                                         reinterpret_cast<void *>(const_cast<uint8_t *>(data)),
                                         buffer_size,
                                         kCFAllocatorNull,
                                         nullptr,
                                         0,
                                         buffer_size,
                                         0,
                                         &block_buffer);

  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to create block buffer.";
    return false;
  }

  // Make sure block buffer is contiguous.
  CMBlockBufferRef contiguous_buffer = nullptr;
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(
        kCFAllocatorDefault, block_buffer, kCFAllocatorNull, nullptr, 0, 0, 0, &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: " << status;
      CFRelease(block_buffer);
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    block_buffer = nullptr;
  }

  // Create sample buffer.
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                contiguous_buffer,
                                true,
                                nullptr,
                                nullptr,
                                video_format,
                                1,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                out_sample_buffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create sample buffer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  CFRelease(contiguous_buffer);
  return true;
}

@implementation RTC_OBJC_TYPE (RTCVideoVTDecoderAV1) {
  std::unique_ptr<libgav1::BufferPool> _buffer_pool;
  std::unique_ptr<libgav1::DecoderState> _decoder_state;
  std::unique_ptr<libgav1::ObuParser> _obuParser;
}

+ (bool)isSupported {
#if defined(RTC_USE_VIDEOTOOLBOX_AV1_DECODER)
  bool isSupported = VTIsHardwareDecodeSupported(kCMVideoCodecType_AV1);
  if (!isSupported) {
    RTC_LOG(LS_WARNING)
        << "Only the Apple M3 chip and A17 Pro chip (and above) support AV1 hardware decoding.";
  }
  return isSupported;
#else
  return false;
#endif
}

- (instancetype _Nullable)init {
  if (![RTCVideoVTDecoderAV1 isSupported]) {
    return nil;
  }

  self = [super init];
  if (self) {
    _decoder_state = std::make_unique<libgav1::DecoderState>();
    _buffer_pool.reset(new libgav1::BufferPool(nullptr, nullptr, nullptr, nullptr));
  }
  return self;
}

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

  if (inputImage.frameType == RTCFrameTypeVideoFrameKey) {
    CMVideoFormatDescriptionRef inputFormat;
    libgav1::RefCountedBufferPtr current_frame;
    _obuParser.reset(new (std::nothrow) libgav1::ObuParser(
        reinterpret_cast<const uint8_t *>(inputImage.buffer.bytes),
        inputImage.buffer.length,
        0,
        _buffer_pool.get(),
        _decoder_state.get()));
    _obuParser->ParseOneFrame(&current_frame);

    int32_t width = 0;
    int32_t height = 0;

    if (current_frame != nullptr) {
      width = current_frame->frame_width();
      height = current_frame->frame_height();
    }

    auto sequenceHeader = _obuParser->sequence_header();
    size_t av1cSize = 0;
    std::unique_ptr<uint8_t[]> av1c = libgav1::ObuParser::GetAV1CodecConfigurationBox(
        reinterpret_cast<const uint8_t *>(inputImage.buffer.bytes),
        inputImage.buffer.length,
        &av1cSize);

    NSDictionary<NSString *, NSObject *> *extension =
        createExtension(sequenceHeader, av1c.get(), av1cSize);

    OSStatus status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                                     kCMVideoCodecType_AV1,
                                                     width,
                                                     height,
                                                     (__bridge CFDictionaryRef)extension,
                                                     &inputFormat);

    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to create video format description.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (inputFormat) {
      // Check if the video format has changed, and reinitialize decoder if
      // needed.
      if (!CMFormatDescriptionEqual(inputFormat, self.videoFormat)) {
        [self setVideoFormat:inputFormat];

        int resetDecompressionSessionError = [self resetDecompressionSession];
        if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
          return resetDecompressionSessionError;
        }
      }
    }
    if (!self.videoFormat) {
      RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  CMSampleBufferRef sampleBuffer = nullptr;
  if (!BufferToCMSampleBuffer(reinterpret_cast<const uint8_t *>(inputImage.buffer.bytes),
                              inputImage.buffer.length,
                              self.videoFormat,
                              &sampleBuffer)) {
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
    kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
  };

  NSDictionary *decoder_config = nil;
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  decoder_config = @{
    (NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder : @YES,
    (NSString *)kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder : @YES,
  };
#endif

  return [super resetDecompressionSession:decoder_config attribute:attributes];
}

@end
