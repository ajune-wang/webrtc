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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/pipewire_stubs.h"
using modules_desktop_capture_linux::InitializeStubs;
using modules_desktop_capture_linux::kModuleDrm;
using modules_desktop_capture_linux::kModulePipewire;
using modules_desktop_capture_linux::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
const char kDrmLib[] = "libdrm.so.2";
#endif

#if !PW_CHECK_VERSION(0, 3, 29)
#define SPA_POD_PROP_FLAG_MANDATORY (1u << 3)
#endif
#if !PW_CHECK_VERSION(0, 3, 33)
#define SPA_POD_PROP_FLAG_DONT_FIXATE (1u << 4)
#endif

struct pw_version {
  int major = 0;
  int minor = 0;
  int micro = 0;
};

bool CheckPipeWireVersion(pw_version required_version) {
  std::vector<std::string> parsed_version;
  std::string version_string = pw_get_library_version();
  rtc::split(version_string, '.', &parsed_version);

  if (parsed_version.size() != 3) {
    return false;
  }

  pw_version current_version = {std::stoi(parsed_version.at(0)),
                                std::stoi(parsed_version.at(1)),
                                std::stoi(parsed_version.at(2))};

  return (current_version.major > required_version.major) ||
         (current_version.major == required_version.major &&
          current_version.minor > required_version.minor) ||
         (current_version.major == required_version.major &&
          current_version.minor == required_version.minor &&
          current_version.micro >= required_version.micro);
}

spa_pod* BuildFormat(spa_pod_builder* builder,
                     uint32_t format,
                     const std::vector<uint64_t>& modifiers) {
  bool first = true;
  spa_pod_frame frames[2];
  spa_rectangle pw_min_screen_bounds = spa_rectangle{1, 1};
  spa_rectangle pw_max_screen_bounds = spa_rectangle{UINT32_MAX, UINT32_MAX};

  spa_pod_builder_push_object(builder, &frames[0], SPA_TYPE_OBJECT_Format,
                              SPA_PARAM_EnumFormat);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaType,
                      SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaSubtype,
                      SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);

  if (modifiers.size()) {
    // SPA_POD_PROP_FLAG_DONT_FIXATE can be used with PipeWire >= 0.3.33
    if (CheckPipeWireVersion(pw_version{0, 3, 33})) {
      spa_pod_builder_prop(
          builder, SPA_FORMAT_VIDEO_modifier,
          SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
    } else {
      spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier,
                           SPA_POD_PROP_FLAG_MANDATORY);
    }
    spa_pod_builder_push_choice(builder, &frames[1], SPA_CHOICE_Enum, 0);
    // modifiers from the array
    for (int64_t val : modifiers) {
      spa_pod_builder_long(builder, val);
      // Add the first modifier twice as the very first value is the default
      // option
      if (first) {
        spa_pod_builder_long(builder, val);
        first = false;
      }
    }
    spa_pod_builder_pop(builder, &frames[1]);
  }

  spa_pod_builder_add(
      builder, SPA_FORMAT_VIDEO_size,
      SPA_POD_CHOICE_RANGE_Rectangle(
          &pw_min_screen_bounds, &pw_min_screen_bounds, &pw_max_screen_bounds),
      0);

  return static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frames[0]));
}

class ScopedBuf {
 public:
  ScopedBuf() {}
  ScopedBuf(uint8_t* map, int map_size, int fd)
      : map_(map), map_size_(map_size), fd_(fd) {}
  ~ScopedBuf() {
    if (map_ != MAP_FAILED) {
      munmap(map_, map_size_);
    }
  }

  operator bool() { return map_ != MAP_FAILED; }

  void initialize(uint8_t* map, int map_size, int fd) {
    map_ = map;
    map_size_ = map_size;
    fd_ = fd;
  }

  uint8_t* get() { return map_; }

 protected:
  uint8_t* map_ = static_cast<uint8_t*>(MAP_FAILED);
  int map_size_;
  int fd_;
};

void BaseCapturerPipeWire::OnCoreError(void* data,
                                       uint32_t id,
                                       int seq,
                                       int res,
                                       const char* message) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
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
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_CONNECTING:
      break;
  }
}

// static
void BaseCapturerPipeWire::OnStreamParamChanged(void* data,
                                                uint32_t id,
                                                const struct spa_pod* format) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";

  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  spa_format_video_raw_parse(format, &that->spa_video_format_);

  auto width = that->spa_video_format_.size.width;
  auto height = that->spa_video_format_.size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  that->desktop_size_ = DesktopSize(width, height);
#if PW_CHECK_VERSION(0, 3, 0)
  that->modifier_ = that->spa_video_format_.modifier;
#endif

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.
  const struct spa_pod* params[3];
  const int buffer_types =
      spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier)
          ? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) |
                (1 << SPA_DATA_MemPtr)
          : (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);
  params[0] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_size, SPA_POD_Int(size), SPA_PARAM_BUFFERS_stride,
      SPA_POD_Int(stride), SPA_PARAM_BUFFERS_buffers,
      SPA_POD_CHOICE_RANGE_Int(8, 1, 32), SPA_PARAM_BUFFERS_dataType,
      SPA_POD_CHOICE_FLAGS_Int(buffer_types)));
  params[1] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header))));
  params[2] = reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_VideoCrop), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_region))));
  pw_stream_update_params(that->pw_stream_, params, 3);
}

// static
void BaseCapturerPipeWire::OnStreamProcess(void* data) {
  BaseCapturerPipeWire* that = static_cast<BaseCapturerPipeWire*>(data);
  RTC_DCHECK(that);

  struct pw_buffer* next_buffer;
  struct pw_buffer* buffer = nullptr;

  next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);
  while (next_buffer) {
    buffer = next_buffer;
    next_buffer = pw_stream_dequeue_buffer(that->pw_stream_);

    if (next_buffer) {
      pw_stream_queue_buffer(that->pw_stream_, buffer);
    }
  }

  if (!buffer) {
    return;
  }

  that->HandleBuffer(buffer);

  pw_stream_queue_buffer(that->pw_stream_, buffer);
}

BaseCapturerPipeWire::BaseCapturerPipeWire() {
  xdg_desktop_portal_ = std::make_unique<XdgDesktopPortal>(
      XdgDesktopPortal::CaptureSourceType::kAnyScreenContent);
}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  if (pw_stream_) {
    pw_stream_destroy(pw_stream_);
  }

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
  }
}

void BaseCapturerPipeWire::OnPortalResponse(bool result) {
  if (result) {
    Init();
    RTC_LOG(LS_INFO) << "XdgDesktopPortal call successfully finished.";
  } else {
    RTC_LOG(LS_INFO) << "XdgDesktopPortal failed.";
  }
}

void BaseCapturerPipeWire::InitPortal() {
  auto callback = std::bind(&BaseCapturerPipeWire::OnPortalResponse, this,
                            std::placeholders::_1);
  xdg_desktop_portal_->InitPortal(callback);
}

void BaseCapturerPipeWire::Init() {
#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire and DRM libraries are available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  paths[kModuleDrm].push_back(kDrmLib);
  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR) << "Failed to load the PipeWire library and symbols.";
    capturer_failed_ = true;
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  egl_dmabuf_ = std::make_unique<EglDmaBuf>();

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);

  pw_thread_loop_lock(pw_main_loop_);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
    return;
  }

  pw_core_ = pw_context_connect_fd(
      pw_context_, xdg_desktop_portal_->PipeWireFileDescriptor().value(),
      nullptr, 0);
  if (!pw_core_) {
    RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
    return;
  }

  // Initialize event handlers, remote end and stream-related.
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;
  pw_stream_events_.process = &OnStreamProcess;

  pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

  pw_stream_ = CreateReceivingStream();
  if (!pw_stream_) {
    RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
    return;
  }

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
    capturer_failed_ = true;
  }

  pw_thread_loop_unlock(pw_main_loop_);

  RTC_LOG(LS_INFO) << "PipeWire remote opened.";
}

pw_stream* BaseCapturerPipeWire::CreateReceivingStream() {
  if (!xdg_desktop_portal_->PipeWireStreamNodeID()) {
    RTC_LOG(LS_ERROR) << "Unable to get PipeWire stream node ID from "
                         "xdg-desktop-portal call.";
    return nullptr;
  }

  pw_properties* reuseProps =
      pw_properties_new_string("pipewire.client.reuse=1");
  auto stream = pw_stream_new(pw_core_, "webrtc-consume-stream", reuseProps);

  uint8_t buffer[2048] = {};
  std::vector<uint64_t> modifiers;

  spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

  std::vector<const spa_pod*> params;
  const bool has_required_pw_version =
      CheckPipeWireVersion(pw_version{0, 3, 29});
  for (uint32_t format : {SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA,
                          SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx}) {
    // Modifiers can be used with PipeWire >= 0.3.29
    if (has_required_pw_version) {
      modifiers = egl_dmabuf_->QueryDmaBufModifiers(format);

      if (!modifiers.empty()) {
        params.push_back(BuildFormat(&builder, format, modifiers));
      }
    }

    params.push_back(BuildFormat(&builder, format, /*modifiers=*/{}));
  }

  pw_stream_add_listener(stream, &spa_stream_listener_, &pw_stream_events_,
                         this);
  if (pw_stream_connect(stream, PW_DIRECTION_INPUT,
                        xdg_desktop_portal_->PipeWireStreamNodeID().value(),
                        PW_STREAM_FLAG_AUTOCONNECT, params.data(),
                        params.size()) != 0) {
    RTC_LOG(LS_ERROR) << "Could not connect receiving stream.";
    capturer_failed_ = true;
    return nullptr;
  }

  return stream;
}

void BaseCapturerPipeWire::HandleBuffer(pw_buffer* buffer) {
  spa_buffer* spa_buffer = buffer->buffer;
  ScopedBuf map;
  std::unique_ptr<uint8_t[]> src_unique_ptr;
  uint8_t* src = nullptr;

  if (spa_buffer->datas[0].chunk->size == 0) {
    RTC_LOG(LS_ERROR) << "Failed to get video stream: Zero size.";
    return;
  }

  if (spa_buffer->datas[0].type == SPA_DATA_MemFd) {
    map.initialize(
        static_cast<uint8_t*>(
            mmap(nullptr,
                 spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
                 PROT_READ, MAP_PRIVATE, spa_buffer->datas[0].fd, 0)),
        spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
        spa_buffer->datas[0].fd);

    if (!map) {
      RTC_LOG(LS_ERROR) << "Failed to mmap the memory: "
                        << std::strerror(errno);
      return;
    }

    src = SPA_MEMBER(map.get(), spa_buffer->datas[0].mapoffset, uint8_t);
  } else if (spa_buffer->datas[0].type == SPA_DATA_DmaBuf) {
    const uint n_planes = spa_buffer->n_datas;
    int fds[n_planes];
    uint32_t offsets[n_planes];
    uint32_t strides[n_planes];

    for (uint32_t i = 0; i < n_planes; ++i) {
      fds[i] = spa_buffer->datas[i].fd;
      offsets[i] = spa_buffer->datas[i].chunk->offset;
      strides[i] = spa_buffer->datas[i].chunk->stride;
    }

    src_unique_ptr = egl_dmabuf_->ImageFromDmaBuf(
        desktop_size_, spa_video_format_.format, n_planes, fds, strides,
        offsets, modifier_);
    src = src_unique_ptr.get();
  } else if (spa_buffer->datas[0].type == SPA_DATA_MemPtr) {
    src = static_cast<uint8_t*>(spa_buffer->datas[0].data);
  }

  if (!src) {
    return;
  }

  struct spa_meta_region* video_metadata =
      static_cast<struct spa_meta_region*>(spa_buffer_find_meta_data(
          spa_buffer, SPA_META_VideoCrop, sizeof(*video_metadata)));

  // Video size from metadata is bigger than an actual video stream size.
  // The metadata are wrong or we should up-scale the video...in both cases
  // just quit now.
  if (video_metadata && (video_metadata->region.size.width >
                             static_cast<uint32_t>(desktop_size_.width()) ||
                         video_metadata->region.size.height >
                             static_cast<uint32_t>(desktop_size_.height()))) {
    RTC_LOG(LS_ERROR) << "Stream metadata sizes are wrong!";
    return;
  }

  // Use video metadata when video size from metadata is set and smaller than
  // video stream size, so we need to adjust it.
  bool video_metadata_use = false;
  const struct spa_rectangle* video_metadata_size =
      video_metadata ? &video_metadata->region.size : nullptr;

  if (video_metadata_size && video_metadata_size->width != 0 &&
      video_metadata_size->height != 0 &&
      (static_cast<int>(video_metadata_size->width) < desktop_size_.width() ||
       static_cast<int>(video_metadata_size->height) <
           desktop_size_.height())) {
    video_metadata_use = true;
  }

  if (video_metadata_use) {
    video_size_ =
        DesktopSize(video_metadata_size->width, video_metadata_size->height);
  } else {
    video_size_ = desktop_size_;
  }

  uint32_t y_offset = video_metadata_use && (video_metadata->region.position.y +
                                                 video_size_.height() <=
                                             desktop_size_.height())
                          ? video_metadata->region.position.y
                          : 0;
  uint32_t x_offset = video_metadata_use && (video_metadata->region.position.x +
                                                 video_size_.width() <=
                                             desktop_size_.width())
                          ? video_metadata->region.position.x
                          : 0;

  webrtc::MutexLock lock(&current_frame_lock_);

  uint8_t* updated_src = src + (spa_buffer->datas[0].chunk->stride * y_offset) +
                         (kBytesPerPixel * x_offset);
  current_frame_ = std::make_unique<BasicDesktopFrame>(
      DesktopSize(video_size_.width(), video_size_.height()));
  current_frame_->CopyPixelsFrom(
      updated_src,
      (spa_buffer->datas[0].chunk->stride - (kBytesPerPixel * x_offset)),
      DesktopRect::MakeWH(video_size_.width(), video_size_.height()));

  if (spa_video_format_.format == SPA_VIDEO_FORMAT_RGBx ||
      spa_video_format_.format == SPA_VIDEO_FORMAT_RGBA) {
    uint8_t* tmp_src = current_frame_->data();
    for (int i = 0; i < video_size_.height(); ++i) {
      // If both sides decided to go with the RGBx format we need to convert it
      // to BGRx to match color format expected by WebRTC.
      ConvertRGBxToBGRx(tmp_src, current_frame_->stride());
      tmp_src += current_frame_->stride();
    }
  }
}

void BaseCapturerPipeWire::ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
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

  InitPortal();

  callback_ = callback;
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (capturer_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  webrtc::MutexLock lock(&current_frame_lock_);
  if (!current_frame_ || !current_frame_->data()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on the
  // frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(current_frame_));
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
std::unique_ptr<DesktopCapturer> BaseCapturerPipeWire::CreateRawCapturer(
    const DesktopCaptureOptions& options) {
  return std::make_unique<BaseCapturerPipeWire>();
}

}  // namespace webrtc
