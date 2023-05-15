/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/blank_detector_desktop_capturer_wrapper.h"

#include <stdint.h>

#include <utility>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

BlankDetectorDesktopCapturerWrapper::BlankDetectorDesktopCapturerWrapper(
    std::unique_ptr<DesktopCapturer> capturer,
    RgbaColor blank_pixel,
    bool check_per_capture)
    : capturer_(std::move(capturer)),
      blank_pixel_(blank_pixel),
      check_per_capture_(check_per_capture) {
  RTC_DCHECK(capturer_);
}

BlankDetectorDesktopCapturerWrapper::~BlankDetectorDesktopCapturerWrapper() =
    default;

void BlankDetectorDesktopCapturerWrapper::Start(
    DesktopCapturer::Callback* callback) {
  callback_ = callback;
  RTC_LOG(LS_ERROR) << "@@Start: " << this << ", cb:" << callback;
  RTC_LOG(LS_ERROR) << "@@Start dc: " << dynamic_cast<DesktopCapturer::Callback*>(this);
  RTC_LOG(LS_ERROR) << "@@Start capturer: " << capturer_.get();
  capturer_->Start(this);
}

void BlankDetectorDesktopCapturerWrapper::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
}

void BlankDetectorDesktopCapturerWrapper::CaptureFrame() {
  RTC_DCHECK(callback_);
  capturer_->CaptureFrame();
}

void BlankDetectorDesktopCapturerWrapper::SetExcludedWindow(WindowId window) {
  capturer_->SetExcludedWindow(window);
}

bool BlankDetectorDesktopCapturerWrapper::GetSourceList(SourceList* sources) {
  return capturer_->GetSourceList(sources);
}

bool BlankDetectorDesktopCapturerWrapper::SelectSource(SourceId id) {
  if (check_per_capture_) {
    // If we start capturing a new source, we must reset these members
    // so we don't short circuit the blank detection logic.
    is_first_frame_ = true;
    non_blank_frame_received_ = false;
  }

  return capturer_->SelectSource(id);
}

bool BlankDetectorDesktopCapturerWrapper::FocusOnSelectedSource() {
  return capturer_->FocusOnSelectedSource();
}

bool BlankDetectorDesktopCapturerWrapper::IsOccluded(const DesktopVector& pos) {
  return capturer_->IsOccluded(pos);
}

void BlankDetectorDesktopCapturerWrapper::OnCaptureResult(
    Result result,
    std::unique_ptr<DesktopFrame> frame) {
  RTC_DCHECK(callback_);
  RTC_LOG(LS_ERROR) << "@BlankD onCaptureResult:" << (int) result
      << " , " << frame.get();
  RTC_LOG(LS_ERROR) << "@BlankD is_texture:" << frame->is_texture()
      << " , handle: " << frame->handle();
  
  if (result != Result::SUCCESS || non_blank_frame_received_) {
    callback_->OnCaptureResult(result, std::move(frame));
    return;
  }
  ///////
  // if (result == Result::SUCCESS) {
  //   callback_->OnCaptureResult(result, std::move(frame));
  //   return;
  // }/////

  if (!frame) {
    // Capturer can call the blank detector with empty frame. Blank
    // detector regards it as a blank frame.
    RTC_LOG(LS_ERROR) << "@BlankD is blank";

    callback_->OnCaptureResult(Result::ERROR_TEMPORARY,
                               std::unique_ptr<DesktopFrame>());
    return;
  }

  // If nothing has been changed in current frame, we do not need to check it
  // again.
  if (!frame->updated_region().is_empty() || is_first_frame_) {
    RTC_LOG(LS_ERROR) << "@BlankD in empty:" << frame->is_texture();
    if (frame->is_texture()) {
      last_frame_is_blank_ = false;
    } else {
      last_frame_is_blank_ = IsBlankFrame(*frame);
    }
    is_first_frame_ = false;
  }
  RTC_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.BlankFrameDetected",
                        last_frame_is_blank_);
  if (!last_frame_is_blank_) {
    non_blank_frame_received_ = true;
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
    return;
  }

RTC_LOG(LS_ERROR) << "@BlankD error";
  callback_->OnCaptureResult(Result::ERROR_TEMPORARY,
                             std::unique_ptr<DesktopFrame>());
}

bool BlankDetectorDesktopCapturerWrapper::IsBlankFrame(
    const DesktopFrame& frame) const {
  // We will check 7489 pixels for a frame with 1024 x 768 resolution.
  for (int i = 0; i < frame.size().width() * frame.size().height(); i += 105) {
    const int x = i % frame.size().width();
    const int y = i / frame.size().width();
    if (!IsBlankPixel(frame, x, y)) {
      return false;
    }
  }

  // We are verifying the pixel in the center as well.
  return IsBlankPixel(frame, frame.size().width() / 2,
                      frame.size().height() / 2);
}

bool BlankDetectorDesktopCapturerWrapper::IsBlankPixel(
    const DesktopFrame& frame,
    int x,
    int y) const {
  uint8_t* pixel_data = frame.GetFrameDataAtPos(DesktopVector(x, y));
  return RgbaColor(pixel_data) == blank_pixel_;
}

}  // namespace webrtc
