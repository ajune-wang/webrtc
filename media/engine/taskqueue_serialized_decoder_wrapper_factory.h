/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MEDIA_ENGINE_TASKQUEUE_SERIALIZED_DECODER_WRAPPER_FACTORY_H_
#define MEDIA_ENGINE_TASKQUEUE_SERIALIZED_DECODER_WRAPPER_FACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

class TaskQueueSerializedDecoderWrapperFactory : public VideoDecoderFactory {
 public:
  TaskQueueSerializedDecoderWrapperFactory(
      TaskQueueFactory* taskqueue_factory,
      std::unique_ptr<VideoDecoderFactory> decoder_factory);

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;

 private:
  const std::unique_ptr<VideoDecoderFactory> decoder_factory_;
  rtc::TaskQueue task_queue_;
};

}  // namespace webrtc
#endif  // MEDIA_ENGINE_TASKQUEUE_SERIALIZED_DECODER_WRAPPER_FACTORY_H_
