#include "modules/video_coding/codecs/h264/openh264/openh264_decoder_impl.h"

#include <algorithm>
#include <limits>

#include "absl/memory/memory.h"
#include "api/video/color_space.h"
#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "modules/video_coding/codecs/h264/h264_color_space.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace webrtc {

enum H264DecoderImplEvent {
  kH264DecoderEventInit = 0,
  kH264DecoderEventError = 1,
  kH264DecoderEventMax = 16,
};

OPENH264DecoderImpl::OPENH264DecoderImpl()
    : pool_(true),
      decoded_image_callback_(nullptr),
      has_reported_init_(false),
      has_reported_error_(false),
      decoder_(NULL) {}

OPENH264DecoderImpl::~OPENH264DecoderImpl() {
  Release();
}

int32_t OPENH264DecoderImpl::InitDecode(const VideoCodec* codec_settings,
                                        int32_t number_of_cores) {
  if (WelsCreateDecoder(&decoder_) != 0) {
    RTC_LOG(LS_ERROR) << "Couldn't create decoder";
    Release();
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (!decoder_) {
    RTC_LOG(LS_ERROR) << "Couldn't create decoder";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  SDecodingParam param;
  memset(&param, 0, sizeof(param));
  param.uiTargetDqLayer = UCHAR_MAX;  // Default value
  param.eEcActiveIdc =
      ERROR_CON_SLICE_MV_COPY_CROSS_IDR_FREEZE_RES_CHANGE;  // Error concealment
                                                            // on.
  param.sVideoProperty.size = sizeof(param.sVideoProperty);
  param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
  if (decoder_->Initialize(&param)) {
    RTC_LOG(LS_ERROR) << "Couldn't initialize decoder";
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_LOG(LS_INFO) << "INITDECODE EST OK";
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* OPENH264DecoderImpl::ImplementationName() const {
  return "OpenH264";
}

bool OPENH264DecoderImpl::IsInitialized() const {
  return decoder_ != nullptr;
}

int32_t OPENH264DecoderImpl::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  if (decoder_ != NULL) {
    if (IsInitialized()) {
      decoder_->Uninitialize();
      WelsDestroyDecoder(decoder_);
      ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
    }
  }
  decoder_ = NULL;
  return ret_val;
}

int32_t OPENH264DecoderImpl::Decode(
    const EncodedImage& input_image,
    bool /*missing_frames*/,
    const CodecSpecificInfo* codec_specific_info,
    int64_t /*render_time_ms*/) {
  if (!IsInitialized()) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!decoded_image_callback_) {
    RTC_LOG(LS_WARNING)
        << "InitDecode() has been called, but a callback function "
           "has not been set with RegisterDecodeCompleteCallback()";
    ReportError();
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!input_image.data() || !input_image.size()) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_specific_info &&
      codec_specific_info->codecType != kVideoCodecH264) {
    ReportError();
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // FFmpeg requires padding due to some optimized bitstream readers reading 32
  // or 64 bits at once and could read over the end. See avcodec_decode_video2.
  RTC_CHECK_GE(input_image.capacity(),
               input_image.size() +
                   EncodedImage::GetBufferPaddingBytes(kVideoCodecH264));
  // "If the first 23 bits of the additional bytes are not 0, then damaged MPEG
  // bitstreams could cause overread and segfault." See
  // AV_INPUT_BUFFER_PADDING_SIZE. We'll zero the entire padding just in case.
  memset(input_image.data() + input_image.size(), 0,
         EncodedImage::GetBufferPaddingBytes(kVideoCodecH264));

  SBufferInfo decoded;
  memset(&decoded, 0, sizeof(SBufferInfo));
  unsigned char* data[3] = {NULL};
  int64_t frame_timestamp_us = input_image.ntp_time_ms_ * 1000;
  const uint8_t* bufferi = input_image.data();
  decoder_->DecodeFrame2(bufferi, input_image.size(), data, &decoded);
  if (!data[0]) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  int width = decoded.UsrData.sSystemBuffer.iWidth;
  int height = decoded.UsrData.sSystemBuffer.iHeight;
  rtc::scoped_refptr<I420Buffer> buffer = pool_.CreateBuffer(width, height);

  libyuv::I420Copy(data[0], decoded.UsrData.sSystemBuffer.iStride[0], data[1],
                   decoded.UsrData.sSystemBuffer.iStride[1], data[2],
                   decoded.UsrData.sSystemBuffer.iStride[1],
                   buffer->MutableDataY(), buffer->StrideY(),
                   buffer->MutableDataU(), buffer->StrideU(),
                   buffer->MutableDataV(), buffer->StrideV(), width, height);

  VideoFrame decoded_image = VideoFrame::Builder()
                                 .set_video_frame_buffer(buffer)
                                 .set_timestamp_us(frame_timestamp_us)
                                 .set_timestamp_rtp(input_image.Timestamp())
                                 .set_color_space(input_image.ColorSpace())
                                 .build();

  decoded_image_callback_->Decoded(decoded_image, absl::nullopt, absl::nullopt);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t OPENH264DecoderImpl::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

void OPENH264DecoderImpl::ReportInit() {
  if (has_reported_init_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264DecoderImpl.Event",
                            kH264DecoderEventInit, kH264DecoderEventMax);
  has_reported_init_ = true;
}

void OPENH264DecoderImpl::ReportError() {
  if (has_reported_error_)
    return;
  RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264DecoderImpl.Event",
                            kH264DecoderEventError, kH264DecoderEventMax);
  has_reported_error_ = true;
}

}  // namespace webrtc