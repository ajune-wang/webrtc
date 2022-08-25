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
#include "modules/desktop_capture/linux/wayland/restore_token_manager.h"
#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/time_utils.h"

#include <unordered_set>

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
              this)) {
  is_screencast_portal_ = true;
}

BaseCapturerPipeWire::BaseCapturerPipeWire(
    const DesktopCaptureOptions& options,
    std::unique_ptr<ScreenCapturePortalInterface> portal)
    : options_(options),
      is_screencast_portal_(false),
      portal_(std::move(portal)) {
  Random random(rtc::TimeMicros());
  source_id_ = static_cast<SourceId>(random.Rand(1, INT_MAX));
}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  RTC_LOG(LS_ERROR) << ">>> Tearing down the base capturer pipewire, stopping "
                    << "screencast streams";
  options_.screencast_stream()->StopScreenCastStream();
}

void BaseCapturerPipeWire::OnScreenCastRequestResult(
    RequestResponse result,
    const SourceStreamInfo& source_stream_info,
    int fd) {
  if (result != RequestResponse::kSuccess) {
    capturer_failed_ = true;
    RTC_LOG(LS_ERROR) << "ScreenCastPortal failed: "
                      << static_cast<uint>(result);
  } else if (ScreenCastPortal* screencast_portal = GetScreenCastPortal()) {
    if (!screencast_portal->RestoreToken().empty()) {
      RestoreTokenManager::GetInstance().AddToken(
          source_id_, screencast_portal->RestoreToken());
    }
  }
  // TODO: Remove this test code
  // static std::unordered_set<std::string>& monitor_names = *new std::unordered_set<std::string>;
  for (auto& [source_id, stream_info] : source_stream_info) {
    // TODO: Hack to ensure that a stream from a single monitor is recorded only
    // once.
    // if (monitor_names.find(stream_info.monitor_name) != monitor_names.end()) continue;
    // monitor_names.insert(stream_info.monitor_name);
    if (!options_.screencast_stream()->StartScreenCastStream(
            stream_info.node_id, fd, options_.get_width(),
            options_.get_height())) {
      capturer_failed_ = true;
      RTC_LOG(LS_ERROR) << "ScreenCastPortal failed start steam: "
                        << stream_info.node_id << ", for source: " << source_id;
      return;
    }
    RTC_LOG(LS_ERROR) << ">>> Storing mapping from source id: " << source_id
                      << ", to stream_id: " << stream_info.node_id;
  }
  pw_fd_ = fd;
  source_stream_info_ = source_stream_info;
  // TODO: Need to figure out a way to determine what is the active source id.
  current_source_id_ = source_stream_info_.begin()->first;
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
  RTC_LOG(LS_INFO) << __func__ << ": >>> Screencast session closed";
  options_.screencast_stream()->StopScreenCastStream();
}

void BaseCapturerPipeWire::UpdateResolution(
    uint32_t width, uint32_t height,
    absl::optional<webrtc::ScreenId> screen_id) {
  if (capturer_failed_) return;
  if (screen_id) {
    if (source_stream_info_.find(*screen_id) == source_stream_info_.end()) {
      RTC_LOG(LS_WARNING) << ">>> Unable to find screen/source id: "
                          << *screen_id << ", skipping resolution update";
      return;
    }
    RTC_LOG(LS_ERROR) << ">>> Updating the stream resolution of screen/source "
                      << "id: " << *screen_id;
    options_.screencast_stream()->UpdateScreenCastStreamResolution(
      width, height, source_stream_info_.find(*screen_id)->second.node_id);
  } else {
    // TODO: Fix update stream resolution story.
    options_.screencast_stream()->UpdateScreenCastStreamResolution(
      width, height);
  }
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  if (ScreenCastPortal* screencast_portal = GetScreenCastPortal()) {
    screencast_portal->SetPersistMode(
        ScreenCastPortal::PersistMode::kTransient);
    if (selected_source_id_) {
      screencast_portal->SetRestoreToken(
          RestoreTokenManager::GetInstance().TakeToken(selected_source_id_));
    }
  }

  portal_->Start();
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (capturer_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  // RTC_LOG(LS_ERROR) << ">>> Capturing frame from source id: "
  //                   << current_source_id_;
  // RTC_DCHECK(current_source_id_ >= 0);
  std::unique_ptr<DesktopFrame> frame;

  if (current_source_id_ == -1) {
    RTC_DCHECK(!source_stream_info_.empty());
    // If all displays are selected then capture from first display.
    // TODO: Check what is the expectation here.
    frame = options_.screencast_stream()->CaptureFrame(
        source_stream_info_.begin()->second.node_id);
  } else {
    auto it = source_stream_info_.find(current_source_id_);
    if (it == source_stream_info_.end()) {
        RTC_LOG(LS_WARNING) << ">>> Stream informatio not found for source: "
          << current_source_id_ << ", not capturing";
        return;
    }
    RTC_DCHECK(it != source_stream_info_.end())
        << ">>> Unknown source id provided: " << current_source_id_;

    frame = options_.screencast_stream()->CaptureFrame(it->second.node_id);
  }

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
  sources->push_back({current_source_id_});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  RTC_LOG(LS_WARNING) << __func__ << ">>> Selecting source id: " << id;
  // options_.screencast_stream()->StopScreenCastStream();

  // TODO: id < 0 for the case where monitor name is not found from the
  // start screencast session request.
  // RTC_DCHECK(id > 0 && source_stream_info_.find(id) !=
  // source_stream_info_.end())
  //  << ">>> Unknown source id provided: " << id
  //  << ", Unable to find corresponding pipewire stream node id";
  current_source_id_ = id;

  // Screen selection is handled by the xdg-desktop-portal.
  selected_source_id_ = id;
  return true;
}

SessionDetails BaseCapturerPipeWire::GetSessionDetails() {
  RTC_LOG(LS_ERROR) << __func__
                    << ">>> Getting session details from the portal";
  SessionDetails session_details = portal_->GetSessionDetails();
  RTC_LOG(LS_ERROR)
      << __func__
      << ">>> Adding active stream information to the session information";
  if (!source_stream_info_.empty()) {
    // Caller is responsible for blocking till the source stream information
    // is available.
    if (current_source_id_ > 0) {
      RTC_LOG(LS_INFO) << ">>> Using current source id: " << current_source_id_
                << ", num stream infos: " << source_stream_info_.size();
      session_details.active_stream =
          source_stream_info_.at(current_source_id_);
    } else {
      session_details.active_stream = source_stream_info_.begin()->second;
    }
    RTC_LOG(LS_ERROR) << __func__ << ">>> Active stream info populated";
  } else {
    RTC_LOG(LS_ERROR) << __func__ << ">>> Active stream info not yet available";
  }
  RTC_LOG(LS_ERROR) << __func__
                    << ">>> Returning the combined session information";
  return session_details;
}

ScreenCastPortal* BaseCapturerPipeWire::GetScreenCastPortal() {
  return is_screencast_portal_ ? static_cast<ScreenCastPortal*>(portal_.get())
                               : nullptr;
}

}  // namespace webrtc
