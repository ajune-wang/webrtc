/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/zero_hertz_encoder_adapter.h"

#include <memory>

#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

class ZeroHertzEncoderAdapterImpl : public ZeroHertzEncoderAdapterInterface {
 public:
  ZeroHertzEncoderAdapterImpl();

  // ZeroHertzEncoderAdapterInterface overrides.
  void Initialize(rtc::VideoSinkInterface<VideoFrame>& sink,
                  Callback& callback) override;
  void SetEnabledByConstraints(
      absl::optional<double> max_framerate_fps) override;
  void SetEnabledByContentType(bool enabled) override;

  // VideoFrameSink overrides.
  void OnFrame(const VideoFrame& frame) override;
  void OnDiscardedFrame() override { sink_->OnDiscardedFrame(); }
  void OnConstraintsChanged(
      const webrtc::VideoTrackSourceConstraints& constraints) override {
    sink_->OnConstraintsChanged(constraints);
  }

 private:
  // True if we support frame entry for screenshare with a minimum frequency of
  // 0 Hz.
  const bool enabled_by_field_trial_;

  // Set up during Init.
  rtc::VideoSinkInterface<VideoFrame>* sink_ = nullptr;
  Callback* callback_ = nullptr;

  // Lock protecting zero-hertz activation state. This is needed because the
  // threading contexts of OnFrame, OnConstraintsChanged, and ConfigureEncoder
  // are mutating it.
  Mutex mutex_;

  // Contains the maximum FPS when zero-hertz mode has been enabled by
  // constraints.
  absl::optional<double> max_framerate_fps_ RTC_GUARDED_BY(mutex_) =
      absl::nullopt;

  // True when zero-hertz mode has been enabled by content type.
  bool enabled_by_content_type_ RTC_GUARDED_BY(mutex_) = false;

  // True when zero-hertz was disabled by constraints or content type.
  bool was_disabled_ RTC_GUARDED_BY(mutex_) = false;
};

ZeroHertzEncoderAdapterImpl::ZeroHertzEncoderAdapterImpl()
    : enabled_by_field_trial_(
          field_trial::IsEnabled("WebRTC-ZeroHertzScreenshare")) {}

void ZeroHertzEncoderAdapterImpl::Initialize(
    rtc::VideoSinkInterface<VideoFrame>& sink,
    Callback& callback) {
  RTC_DCHECK(!sink_);
  RTC_DCHECK(!callback_);
  sink_ = &sink;
  callback_ = &callback;
}

void ZeroHertzEncoderAdapterImpl::SetEnabledByConstraints(
    absl::optional<double> max_framerate_fps) {
  if (!enabled_by_field_trial_)
    return;
  MutexLock lock(&mutex_);
  if (enabled_by_content_type_) {
    if (!max_framerate_fps.has_value() && max_framerate_fps_.has_value())
      was_disabled_ = true;
  }
  max_framerate_fps_ = max_framerate_fps;
}

void ZeroHertzEncoderAdapterImpl::SetEnabledByContentType(bool enabled) {
  if (!enabled_by_field_trial_)
    return;
  MutexLock lock(&mutex_);
  if (max_framerate_fps_.has_value()) {
    if (!enabled && enabled_by_content_type_)
      was_disabled_ = true;
  }
  enabled_by_content_type_ = enabled;
}

void ZeroHertzEncoderAdapterImpl::OnFrame(const VideoFrame& frame) {
  if (enabled_by_field_trial_) {
    MutexLock lock(&mutex_);
    if (was_disabled_)
      callback_->OnZeroHertzModeDeactivated();
    was_disabled_ = false;
  }
  sink_->OnFrame(frame);
}

}  // namespace

std::unique_ptr<ZeroHertzEncoderAdapterInterface>
ZeroHertzEncoderAdapterInterface::Create() {
  return std::make_unique<ZeroHertzEncoderAdapterImpl>();
}

}  // namespace webrtc
