/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/android_video_track_source.h"

#include <utility>

#include "api/video_track_source_proxy.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace jni {

namespace {
// MediaCodec wants resolution to be divisible by 2.
const int kRequiredResolutionAlignment = 2;
}  // namespace

AndroidVideoTrackSource::AndroidVideoTrackSource(rtc::Thread* signaling_thread,
                                                 JNIEnv* jni,
                                                 bool is_screencast,
                                                 bool align_timestamps)
    : AdaptedVideoTrackSource(kRequiredResolutionAlignment),
      signaling_thread_(signaling_thread),
      is_screencast_(is_screencast),
      align_timestamps_(align_timestamps) {
  RTC_LOG(LS_INFO) << "AndroidVideoTrackSource ctor";
}
AndroidVideoTrackSource::~AndroidVideoTrackSource() = default;

bool AndroidVideoTrackSource::is_screencast() const {
  return is_screencast_;
}

absl::optional<bool> AndroidVideoTrackSource::needs_denoising() const {
  return false;
}

void AndroidVideoTrackSource::SetState(SourceState state) {
  if (rtc::Thread::Current() != signaling_thread_) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&AndroidVideoTrackSource::SetState, this, state));
    return;
  }

  if (state_ != state) {
    state_ = state;
    FireOnChanged();
  }
}

AndroidVideoTrackSource::SourceState AndroidVideoTrackSource::state() const {
  return state_;
}

bool AndroidVideoTrackSource::remote() const {
  return false;
}

absl::optional<AndroidVideoTrackSource::FrameAdaptationParameters>
AndroidVideoTrackSource::GetFrameAdaptationParameters(int width,
                                                      int height,
                                                      int64_t timestamp_ns,
                                                      VideoRotation rotation) {
  FrameAdaptationParameters parameters;

  int64_t camera_time_us = timestamp_ns / rtc::kNumNanosecsPerMicrosec;
  parameters.aligned_timestamp_ns =
      align_timestamps_ ? rtc::kNumNanosecsPerMicrosec *
                              timestamp_aligner_.TranslateTimestamp(
                                  camera_time_us, rtc::TimeMicros())
                        : timestamp_ns;

  if (rotation % 180 == 0) {
    if (!AdaptFrame(width, height, camera_time_us, &parameters.adapted_width,
                    &parameters.adapted_height, &parameters.crop_width,
                    &parameters.crop_height, &parameters.crop_x,
                    &parameters.crop_y)) {
      return absl::nullopt;
    }
  } else {
    // Swap all width/height and x/y.
    if (!AdaptFrame(height, width, camera_time_us, &parameters.adapted_height,
                    &parameters.adapted_width, &parameters.crop_height,
                    &parameters.crop_width, &parameters.crop_y,
                    &parameters.crop_x)) {
      return absl::nullopt;
    }
  }

  return parameters;
}

void AndroidVideoTrackSource::OnFrameCaptured(
    JNIEnv* env,
    int64_t timestamp_ns,
    VideoRotation rotation,
    const JavaRef<jobject>& j_video_frame_buffer) {
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      AndroidVideoBuffer::Create(env, j_video_frame_buffer);

  // AdaptedVideoTrackSource handles applying rotation for I420 frames.
  if (apply_rotation() && rotation != kVideoRotation_0)
    buffer = buffer->ToI420();

  OnFrame(VideoFrame::Builder()
              .set_video_frame_buffer(buffer)
              .set_rotation(rotation)
              .set_timestamp_us(timestamp_ns / rtc::kNumNanosecsPerMicrosec)
              .build());
}

void AndroidVideoTrackSource::OnOutputFormatRequest(int landscape_width,
                                                    int landscape_height,
                                                    int portrait_width,
                                                    int portrait_height,
                                                    int fps) {
  video_adapter()->OnOutputFormatRequest(
      std::make_pair(landscape_width, landscape_height),
      landscape_width * landscape_height,
      std::make_pair(portrait_width, portrait_height),
      portrait_width * portrait_height, fps);
}

}  // namespace jni
}  // namespace webrtc
