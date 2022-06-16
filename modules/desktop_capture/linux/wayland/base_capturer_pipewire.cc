/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/base_capturer_pipewire.h"

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

using xdg_portal::RequestResponse;
using xdg_portal::ScreenCapturePortalInterface;
using xdg_portal::SessionDetails;

}  // namespace

BaseCapturerPipeWire::BaseCapturerPipeWire(const DesktopCaptureOptions& options)
    : BaseCapturerPipeWire(
          options,
          std::make_unique<ScreenCastPortal>(
              ScreenCastPortal::CaptureSourceType::kAnyScreenContent,
              this)) {}

BaseCapturerPipeWire::BaseCapturerPipeWire(
    const DesktopCaptureOptions& options,
    std::unique_ptr<ScreenCapturePortalInterface> portal)
    : options_(options), portal_(std::move(portal)) {}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {}

void BaseCapturerPipeWire::OnScreenCastRequestResult(
    RequestResponse result,
    const SourceStreamIds& stream_node_ids,
    int fd) {
  if (result != RequestResponse::kSuccess) {
    capturer_failed_ = true;
    RTC_LOG(LS_ERROR) << "ScreenCastPortal failed: "
                      << static_cast<uint>(result);
    return;
  }
  for (auto& [source_id, stream_id] : stream_node_ids) {
    if (!options_.screencast_stream()->StartScreenCastStream(
            stream_id, fd, options_.get_width(), options_.get_height())) {
      capturer_failed_ = true;
      RTC_LOG(LS_ERROR) << "ScreenCastPortal failed start steam: " << stream_id
                        << ", for source: " << source_id;
      return;
    }
    RTC_LOG(LS_ERROR) << ">>> Storing mapping from source id: " << source_id
                      << ", to stream_id: " << stream_id;
    source_to_node_id_[source_id] = stream_id;
  }
  pw_fd_ = fd;
  SourceStreamIds stream_node_ids_ = stream_node_ids;
  // TODO: Need to figure out a way to determine what is the active source id.
  current_source_id_ = stream_node_ids_.begin()->first;
  // TODO: Allow for monitor name's absence when starting screencast stream
  // session.
  RTC_LOG(LS_ERROR) << ">>> Current source id: " << current_source_id_;
  // RTC_DCHECK(current_source_id_ >= 0);
  RTC_LOG(LS_ERROR) << ">>> Done Starting screencapture streams, current "
                    << "stream set to: " << current_source_id_;
}

void BaseCapturerPipeWire::OnScreenCastSessionClosed() {
  // if (!capturer_failed_) {
  //  for (auto& [source_id, stream] : options_.screencast_streams())
  //    stream->StopScreenCastStream();
  //}
  options_.screencast_stream()->StopScreenCastStream();
}

void BaseCapturerPipeWire::UpdateResolution(uint32_t width, uint32_t height) {
  if (!capturer_failed_) {
    // RTC_DCHECK(current_source_id_ >= 0);
    // TODO: Fix update stream resolution story.
    // options_.screencast_streams(current_source_id_)->UpdateScreenCastStreamResolution(width,
    //                                                                height);
  }
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  portal_->Start();
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (capturer_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  RTC_LOG(LS_ERROR) << ">>> Capturing frame from source id: "
                    << current_source_id_;
  // RTC_DCHECK(current_source_id_ >= 0);
  auto it = source_to_node_id_.find(current_source_id_);
  RTC_DCHECK(it != source_to_node_id_.end())
      << ">>> Unknown source id provided: " << current_source_id_;

  std::unique_ptr<DesktopFrame> frame =
      options_.screencast_stream()->CaptureFrame(it->second);

  if (!frame || !frame->data()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on
  // the frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal,
  // so we just need a (valid) source id for any callers to pass around, even
  // though it doesn't mean anything to us. Until the user selects a source in
  // xdg-desktop-portal we'll just end up returning empty frames. Note that "0"
  // is often treated as a null/placeholder id, so we shouldn't use that.
  // TODO(https://crbug.com/1297671): Reconsider type of ID when plumbing
  // token that will enable stream re-use.
  sources->push_back({1});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  RTC_LOG(LS_WARNING) << __func__ << ">>> Selecting source id: " << id;
  // options_.screencast_stream()->StopScreenCastStream();

  // TODO: id < 0 for the case where monitor name is not found from the
  // start screencast session request.
  // RTC_DCHECK(id > 0 && source_to_node_id_.find(id) !=
  // source_to_node_id_.end())
  //  << ">>> Unknown source id provided: " << id
  //  << ", Unable to find corresponding pipewire stream node id";
  current_source_id_ = id;

  // options_.screencast_stream(it->second)->StartScreenCastStream(
  //         it->second, pw_fd_, options_.get_width(),
  //         options_.get_height());
  //  Screen selection is handled by the xdg-desktop-portal.
  return true;
}

SessionDetails BaseCapturerPipeWire::GetSessionDetails() {
  return portal_->GetSessionDetails();
}

}  // namespace webrtc
