/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/base_capturer_pipewire.h"

#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/raw-utils.h>
#include <spa/support/type-map.h>

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/pipewire_stubs.h"

using modules_desktop_capture_linux::InitializeStubs;
using modules_desktop_capture_linux::kModulePipewire;
using modules_desktop_capture_linux::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.2.so.1";
#endif

// static
void BaseCapturerPipeWire::OnStateChanged(void* data,
                                          pw_remote_state old_state,
                                          pw_remote_state state,
                                          const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_REMOTE_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire remote state error: " << error_message;
      break;
    case PW_REMOTE_STATE_CONNECTED:
      RTC_LOG(LS_INFO) << "PipeWire remote state: connected.";
      that->CreateReceivingStream();
      break;
    case PW_REMOTE_STATE_CONNECTING:
      RTC_LOG(LS_INFO) << "PipeWire remote state: connecting.";
      break;
    case PW_REMOTE_STATE_UNCONNECTED:
      RTC_LOG(LS_INFO) << "PipeWire remote state: unconnected.";
      break;
  }
}

// static
void BaseCapturerPipeWire::OnStreamStateChanged(void* data,
                                                pw_stream_state old_state,
                                                pw_stream_state state,
                                                const char* error_message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire stream state error: " << error_message;
      break;
    case PW_STREAM_STATE_CONFIGURE:
      pw_stream_set_active(that->pw_stream_, true);
      break;
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
    case PW_STREAM_STATE_READY:
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
      break;
  }
}

// static
void BaseCapturerPipeWire::OnStreamFormatChanged(void* data,
                                                 const struct spa_pod* format) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";

  if (!format) {
    pw_stream_finish_format(that->pw_stream_, /*res=*/0, /*params=*/nullptr,
                            /*n_params=*/0);
    return;
  }

  that->spa_video_format_ = new spa_video_info_raw();
  spa_format_video_raw_parse(format, that->spa_video_format_,
                             &that->pw_type_->format_video);

  auto width = that->spa_video_format_->size.width;
  auto height = that->spa_video_format_->size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[3];
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate buffer requirements
      that->pw_core_type_->param.idBuffers,
      that->pw_core_type_->param_buffers.Buffers,
      // Size: specified as integer (i) and set to specified size
      ":", that->pw_core_type_->param_buffers.size, "i", size,
      // Stride: specified as integer (i) and set to specified stride
      ":", that->pw_core_type_->param_buffers.stride, "i", stride,
      // Buffers: specifies how many buffers we want to deal with, set as
      // integer (i) where preferred number is 8, then allowed number is defined
      // as range (r) from min and max values and it is undecided (u) to allow
      // negotiation
      ":", that->pw_core_type_->param_buffers.buffers, "iru", 8,
      SPA_POD_PROP_MIN_MAX(1, 32),
      // Align: memory alignment of the buffer, set as integer (i) to specified
      // value
      ":", that->pw_core_type_->param_buffers.align, "i", 16));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate supported metadata
      that->pw_core_type_->param.idMeta, that->pw_core_type_->param_meta.Meta,
      // Type: specified as id or enum (I)
      ":", that->pw_core_type_->param_meta.type, "I",
      that->pw_core_type_->meta.Header,
      // Size: size of the metadata, specified as integer (i)
      ":", that->pw_core_type_->param_meta.size, "i",
      sizeof(struct spa_meta_header)));
  params[2] = reinterpret_cast<spa_pod*>(
      spa_pod_builder_object(&builder, that->pw_core_type_->param.idMeta,
                             that->pw_core_type_->param_meta.Meta, ":",
                             that->pw_core_type_->param_meta.type, "I",
                             that->pw_core_type_->meta.VideoCrop, ":",
                             that->pw_core_type_->param_meta.size, "i",
                             sizeof(struct spa_meta_video_crop)));
  pw_stream_finish_format(that->pw_stream_, /*res=*/0, params, /*n_params=*/3);
}

// static
void BaseCapturerPipeWire::OnStreamProcess(void* data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  pw_buffer* buf = nullptr;

  if (!(buf = pw_stream_dequeue_buffer(that->pw_stream_))) {
    return;
  }

  that->HandleBuffer(buf);

  pw_stream_queue_buffer(that->pw_stream_, buf);
}

BaseCapturerPipeWire::BaseCapturerPipeWire() {}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  if (connection_id_) {
    webrtc::XdgDesktopPortalBase* xdpBase = options_.xdp_base();
    xdpBase->CloseConnection(connection_id_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  if (pw_type_) {
    delete pw_type_;
  }

  if (spa_video_format_) {
    delete spa_video_format_;
  }

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

  if (pw_remote_) {
    pw_remote_destroy(pw_remote_);
  }

  if (pw_core_) {
    pw_core_destroy(pw_core_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }

  if (pw_loop_) {
    pw_loop_destroy(pw_loop_);
  }

  if (current_frame_) {
    delete[] current_frame_;
  }
}

bool BaseCapturerPipeWire::Init(const DesktopCaptureOptions& options) {
  options_ = options;

  return true;
}

void BaseCapturerPipeWire::InitPipeWire() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire library is available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    pipewire_init_failed_ = true;
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_loop_ = pw_loop_new(/*properties=*/nullptr);
  pw_main_loop_ = pw_thread_loop_new(pw_loop_, "pipewire-main-loop");

  pw_core_ = pw_core_new(pw_loop_, /*properties=*/nullptr);
  pw_core_type_ = pw_core_get_type(pw_core_);
  pw_remote_ = pw_remote_new(pw_core_, nullptr, /*user_data_size=*/0);

  InitPipeWireTypes();

  // Initialize event handlers, remote end and stream-related.
  pw_remote_events_.version = PW_VERSION_REMOTE_EVENTS;
  pw_remote_events_.state_changed = &OnStateChanged;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.format_changed = &OnStreamFormatChanged;
  pw_stream_events_.process = &OnStreamProcess;

  pw_remote_add_listener(pw_remote_, &spa_remote_listener_, &pw_remote_events_,
                         this);
  pw_remote_connect_fd(pw_remote_, pw_fd_);

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    pipewire_init_failed_ = true;
  }

  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

void BaseCapturerPipeWire::InitPipeWireTypes() {
  spa_type_map* map = pw_core_type_->map;
  pw_type_ = new PipeWireType();

  spa_type_media_type_map(map, &pw_type_->media_type);
  spa_type_media_subtype_map(map, &pw_type_->media_subtype);
  spa_type_format_video_map(map, &pw_type_->format_video);
  spa_type_video_format_map(map, &pw_type_->video_format);
}

void BaseCapturerPipeWire::CreateReceivingStream() {
  spa_rectangle pwMinScreenBounds = spa_rectangle{1, 1};
  spa_rectangle pwScreenBounds =
      spa_rectangle{static_cast<uint32_t>(desktop_size_.width()),
                    static_cast<uint32_t>(desktop_size_.height())};

  spa_fraction pwFrameRateMin = spa_fraction{0, 1};
  spa_fraction pwFrameRateMax = spa_fraction{60, 1};

  pw_properties* reuseProps =
      pw_properties_new_string("pipewire.client.reuse=1");
  pw_stream_ = pw_stream_new(pw_remote_, "webrtc-consume-stream", reuseProps);

  uint8_t buffer[1024] = {};
  const spa_pod* params[1];
  spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_object(
      &builder,
      // id to enumerate formats
      pw_core_type_->param.idEnumFormat, pw_core_type_->spa_format, "I",
      pw_type_->media_type.video, "I", pw_type_->media_subtype.raw,
      // Video format: specified as id or enum (I), preferred format is BGRx,
      // then allowed formats are enumerated (e) and the format is undecided (u)
      // to allow negotiation
      ":", pw_type_->format_video.format, "Ieu", pw_type_->video_format.BGRx,
      SPA_POD_PROP_ENUM(2, pw_type_->video_format.RGBx,
                        pw_type_->video_format.BGRx),
      // Video size: specified as rectangle (R), preferred size is specified as
      // first parameter, then allowed size is defined as range (r) from min and
      // max values and the format is undecided (u) to allow negotiation
      ":", pw_type_->format_video.size, "Rru", &pwScreenBounds, 2,
      &pwMinScreenBounds, &pwScreenBounds,
      // Frame rate: specified as fraction (F) and set to minimum frame rate
      // value
      ":", pw_type_->format_video.framerate, "F", &pwFrameRateMin,
      // Max frame rate: specified as fraction (F), preferred frame rate is set
      // to maximum value, then allowed frame rate is defined as range (r) from
      // min and max values and it is undecided (u) to allow negotiation
      ":", pw_type_->format_video.max_framerate, "Fru", &pwFrameRateMax, 2,
      &pwFrameRateMin, &pwFrameRateMax));

  pw_stream_add_listener(pw_stream_, &spa_stream_listener_, &pw_stream_events_,
                         this);
  pw_stream_flags flags = static_cast<pw_stream_flags>(
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE |
      PW_STREAM_FLAG_MAP_BUFFERS);
  if (pw_stream_connect(pw_stream_, PW_DIRECTION_INPUT, /*port_path=*/nullptr,
                        flags, params,
                        /*n_params=*/1) != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    pipewire_init_failed_ = true;
    return;
  }
}

void BaseCapturerPipeWire::HandleBuffer(pw_buffer* buffer) {
  struct spa_meta_video_crop* videoCrop;
  spa_buffer* spaBuffer = buffer->buffer;
  uint8_t* src = nullptr;
  uint8_t* dst = nullptr;

  if (!(src = static_cast<uint8_t*>(spaBuffer->datas[0].data))) {
    return;
  }

  DesktopSize prevCropSize = video_crop_size_.value_or(DesktopSize(0, 0));

  if ((videoCrop = static_cast<struct spa_meta_video_crop*>(
           spa_buffer_find_meta(spaBuffer, pw_core_type_->meta.VideoCrop)))) {
    video_crop_size_ = DesktopSize(videoCrop->width, videoCrop->height);
    RTC_DCHECK(video_crop_size_->width() <= desktop_size_.width() &&
               video_crop_size_->height() <= desktop_size_.height());
    use_video_crop_ = !video_crop_size_->equals(desktop_size_);
  } else {
    use_video_crop_ = false;
  }

  size_t frameSize;
  if (video_crop_size_) {
    frameSize =
        video_crop_size_->width() * video_crop_size_->height() * kBytesPerPixel;
  } else {
    frameSize = desktop_size_.width() * desktop_size_.height() * kBytesPerPixel;
  }

  if (!current_frame_) {
    current_frame_ = new uint8_t[frameSize];
  } else if (video_crop_size_ && !video_crop_size_->equals(prevCropSize)) {
    if (current_frame_) {
      delete[] current_frame_;
    }
    current_frame_ = new uint8_t[frameSize];
  }
  RTC_DCHECK(current_frame_ != nullptr);

  const int32_t dstStride = use_video_crop_
                                ? video_crop_size_->width() * kBytesPerPixel
                                : desktop_size_.width() * kBytesPerPixel;
  const int32_t srcStride = spaBuffer->datas[0].chunk->stride;

  if (srcStride != (desktop_size_.width() * kBytesPerPixel)) {
    RTC_LOG(LS_ERROR) << "Got buffer with stride different from screen stride: "
                      << srcStride
                      << " != " << (desktop_size_.width() * kBytesPerPixel);
    pipewire_init_failed_ = true;
    return;
  }

  dst = current_frame_;

  // Adjust source content based on crop video position
  if (use_video_crop_ &&
      (videoCrop->y + video_crop_size_->height() <= desktop_size_.height())) {
    for (int i = 0; i < videoCrop->y; ++i) {
      src += srcStride;
    }
  }

  const int xOffset =
      use_video_crop_ && (videoCrop->x + video_crop_size_->width() <=
                          desktop_size_.width())
          ? videoCrop->x * kBytesPerPixel
          : 0;
  const int height =
      use_video_crop_ ? video_crop_size_->height() : desktop_size_.height();
  for (int i = 0; i < height; ++i) {
    // Adjust source content based on crop video position if needed
    src += xOffset;
    std::memcpy(dst, src, dstStride);
    // If both sides decided to go with the RGBx format we need to convert it to
    // BGRx to match color format expected by WebRTC.
    if (spa_video_format_->format == pw_type_->video_format.RGBx) {
      ConvertRGBxToBGRx(dst, dstStride);
    }
    src += srcStride - xOffset;
    dst += dstStride;
  }
}

void BaseCapturerPipeWire::ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
  // Change color format for KDE KWin which uses RGBx and not BGRx
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  auto lambda = [this](bool result, int32_t id) {
    if (result) {
      connection_id_ = id;
      pw_fd_ = options_.xdp_base()->GetPipeWireFd();
      desktop_size_ = options_.xdp_base()->GetDesktopSize();
      InitPipeWire();
    }
  };

  webrtc::XdgDesktopPortalBase* xdpBase = options_.xdp_base();
  rtc::Callback2<void, bool, int32_t> cb = lambda;
  xdpBase->OpenPipeWireRemote(cb);
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (pipewire_init_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (!current_frame_) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  DesktopSize frameSize =
      use_video_crop_ ? video_crop_size_.value() : desktop_size_;
  std::unique_ptr<DesktopFrame> result(new BasicDesktopFrame(frameSize));
  result->CopyPixelsFrom(
      current_frame_, (frameSize.width() * kBytesPerPixel),
      DesktopRect::MakeWH(frameSize.width(), frameSize.height()));
  if (!result) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal.
  // But we have to add an empty source as the code expects it.
  sources->push_back({0});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  return true;
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.xdp_base())
    return nullptr;

  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>(BaseCapturerPipeWire());
  if (!capturer->Init(options)) {
    return nullptr;
  }

  return std::move(capturer);
}

// static
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.xdp_base())
    return nullptr;

  std::unique_ptr<BaseCapturerPipeWire> capturer =
      std::make_unique<BaseCapturerPipeWire>(BaseCapturerPipeWire());
  if (!capturer->Init(options)) {
    return nullptr;
  }

  return std::move(capturer);
}

}  // namespace webrtc
