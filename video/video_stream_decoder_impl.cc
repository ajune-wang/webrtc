/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_decoder_impl.h"

#include "rtc_base/ptr_util.h"

namespace webrtc {

std::unique_ptr<VideoStreamDecoder> VideoStreamDecoder::Create(
    VideoDecoderFactory* decoder_factory,
    std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings,
    std::function<void()> non_decodable_callback,
    std::function<void(FrameKey)> continuous_callback,
    std::function<void(VideoStreamDecoder::DecodedFrameInfo)>
        decoded_callback) {
  return rtc::MakeUnique<VideoStreamDecoderImpl>(
      decoder_factory, std::move(decoder_settings),
      std::move(non_decodable_callback), std::move(continuous_callback),
      std::move(decoded_callback));
}

VideoStreamDecoderImpl::VideoStreamDecoderImpl(
    VideoDecoderFactory* decoder_factory,
    std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings,
    std::function<void()> non_decodable_callback,
    std::function<void(FrameKey)> continuous_callback,
    std::function<void(VideoStreamDecoder::DecodedFrameInfo)> decoded_callback)
    : decoder_factory_(decoder_factory),
      decoder_settings_(std::move(decoder_settings)),
      non_decodable_callback_(std::move(non_decodable_callback)),
      continuous_callback_(std::move(continuous_callback)),
      decoded_callback_(std::move(decoded_callback)) {}

VideoStreamDecoderImpl::~VideoStreamDecoderImpl() {}

void VideoStreamDecoderImpl::OnFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {}

}  // namespace webrtc
