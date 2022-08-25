/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"

#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <sys/mman.h>

#include <unordered_map>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/linux/wayland/egl_dmabuf.h"
#include "modules/desktop_capture/linux/wayland/screencast_stream_utils.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/wayland/pipewire_stubs.h"
using modules_desktop_capture_linux_wayland::InitializeStubs;
using modules_desktop_capture_linux_wayland::kModuleDrm;
using modules_desktop_capture_linux_wayland::kModulePipewire;
using modules_desktop_capture_linux_wayland::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

const int kBytesPerPixel = 4;

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
const char kDrmLib[] = "libdrm.so.2";
#endif

constexpr int kCursorBpp = 4;
constexpr int CursorMetaSize(int w, int h) {
  return (sizeof(struct spa_meta_cursor) + sizeof(struct spa_meta_bitmap) +
          w * h * kCursorBpp);
}

constexpr PipeWireVersion kDmaBufMinVersion = {0, 3, 24};
constexpr PipeWireVersion kDmaBufModifierMinVersion = {0, 3, 33};
constexpr PipeWireVersion kDropSingleModifierMinVersion = {0, 3, 40};

namespace {

struct StreamData {
  // Resolution parameters.
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  std::unique_ptr<webrtc::Mutex> resolution_lock_ =
      std::make_unique<webrtc::Mutex>();
  // Resolution changes are processed during buffer processing.
  bool pending_resolution_change_ RTC_GUARDED_BY(resolution_lock_.get()) =
      false;

  struct pw_stream* pw_stream_ = nullptr;
  uint32_t pw_stream_node_id_ = 0;
  spa_hook spa_stream_listener_;
  struct spa_video_info_raw spa_video_format_;
  DesktopSize stream_size_ = {};
  int64_t modifier_;
  // List of modifiers we query as supported by the graphics card/driver
  std::vector<uint64_t> modifiers_;

  DesktopSize frame_size_;

  std::unique_ptr<webrtc::Mutex> queue_lock_ =
      std::make_unique<webrtc::Mutex>();
  std::unique_ptr<ScreenCaptureFrameQueue<SharedDesktopFrame>> queue_
      RTC_GUARDED_BY(queue_lock_.get()) =
          std::make_unique<ScreenCaptureFrameQueue<SharedDesktopFrame>>();
  std::unique_ptr<MouseCursor> mouse_cursor_;
  DesktopVector mouse_cursor_position_ = DesktopVector(-1, -1);
  struct spa_source* renegotiate_ = nullptr;
};

struct GlobalData {
  EglDmaBuf* egl_dmabuf_;
  // Read-only (Used in `OnStreamParamChanged`)
  PipeWireVersion pw_server_version_;
  // Used for locking in `OnRenegotiateFormat`
  struct pw_thread_loop* pw_main_loop_;
};

// Encapsulates the data (global + stream specific) that is needed by the
// callbacks corresponding to stream events.
struct CallbackData {
  // CallbackData(GlobalData global_data, StreamData stream_data) :
  //   global(std::move(global_data)), stream(std::move(stream_data)) {}
  GlobalData global;
  StreamData stream;
};

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

  explicit operator bool() { return map_ != MAP_FAILED; }

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

void ConvertRGBxToBGRx(uint8_t* frame, uint32_t size) {
  for (uint32_t i = 0; i < size; i += 4) {
    uint8_t tempR = frame[i];
    uint8_t tempB = frame[i + 2];
    frame[i] = tempB;
    frame[i + 2] = tempR;
  }
}

RTC_NO_SANITIZE("cfi-icall")
void ProcessBuffer(pw_buffer* buffer, CallbackData* cb_data) {
  spa_buffer* spa_buffer = buffer->buffer;
  ScopedBuf map;
  std::unique_ptr<uint8_t[]> src_unique_ptr;
  uint8_t* src = nullptr;

  // Try to update the mouse cursor first, because it can be the only
  // information carried by the buffer
  {
    const struct spa_meta_cursor* cursor =
        static_cast<struct spa_meta_cursor*>(spa_buffer_find_meta_data(
            spa_buffer, SPA_META_Cursor, sizeof(*cursor)));
    if (cursor && spa_meta_cursor_is_valid(cursor)) {
      struct spa_meta_bitmap* bitmap = nullptr;

      if (cursor->bitmap_offset)
        bitmap =
            SPA_MEMBER(cursor, cursor->bitmap_offset, struct spa_meta_bitmap);

      if (bitmap && bitmap->size.width > 0 && bitmap->size.height > 0) {
        const uint8_t* bitmap_data =
            SPA_MEMBER(bitmap, bitmap->offset, uint8_t);
        BasicDesktopFrame* mouse_frame = new BasicDesktopFrame(
            DesktopSize(bitmap->size.width, bitmap->size.height));
        mouse_frame->CopyPixelsFrom(
            bitmap_data, bitmap->stride,
            DesktopRect::MakeWH(bitmap->size.width, bitmap->size.height));
        cb_data->stream.mouse_cursor_ = std::make_unique<MouseCursor>(
            mouse_frame, DesktopVector(cursor->hotspot.x, cursor->hotspot.y));
      }
      cb_data->stream.mouse_cursor_position_.set(cursor->position.x,
                                                 cursor->position.y);
    }
  }

  if (spa_buffer->datas[0].chunk->size == 0) {
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

    if (!n_planes) {
      return;
    }

    std::vector<EglDmaBuf::PlaneData> plane_datas;
    for (uint32_t i = 0; i < n_planes; ++i) {
      EglDmaBuf::PlaneData data = {
          static_cast<int32_t>(spa_buffer->datas[i].fd),
          static_cast<uint32_t>(spa_buffer->datas[i].chunk->stride),
          static_cast<uint32_t>(spa_buffer->datas[i].chunk->offset)};
      plane_datas.push_back(data);
    }

    // When importing DMA-BUFs, we use the stride (number of bytes from one row
    // of pixels in the buffer) provided by PipeWire. The stride from PipeWire
    // is given by the graphics driver and some drivers might add some
    // additional padding for memory layout optimizations so not everytime the
    // stride is equal to BYTES_PER_PIXEL x WIDTH. This is fine, because during
    // the import we will use OpenGL and same graphics driver so it will be able
    // to work with the stride it provided, but later on when we work with
    // images we get from DMA-BUFs we will need to update the stride to be equal
    // to BYTES_PER_PIXEL x WIDTH as that's the size of the DesktopFrame we
    // allocate for each captured frame.
    src_unique_ptr = cb_data->global.egl_dmabuf_->ImageFromDmaBuf(
        cb_data->stream.stream_size_, cb_data->stream.spa_video_format_.format,
        plane_datas, cb_data->stream.modifier_);
    if (src_unique_ptr) {
      src = src_unique_ptr.get();
    } else {
      RTC_LOG(LS_ERROR) << "Dropping DMA-BUF modifier: "
                        << cb_data->stream.modifier_
                        << " and trying to renegotiate stream parameters";

      std::vector<uint64_t>& modifiers_ = cb_data->stream.modifiers_;
      if (cb_data->global.pw_server_version_ >= kDropSingleModifierMinVersion) {
        modifiers_.erase(std::remove(modifiers_.begin(), modifiers_.end(),
                                     cb_data->stream.modifier_),
                         modifiers_.end());
      } else {
        modifiers_.clear();
      }

      pw_loop_signal_event(
          pw_thread_loop_get_loop(cb_data->global.pw_main_loop_),
          cb_data->stream.renegotiate_);
      return;
    }
  } else if (spa_buffer->datas[0].type == SPA_DATA_MemPtr) {
    src = static_cast<uint8_t*>(spa_buffer->datas[0].data);
  }

  if (!src) {
    return;
  }

  // Use SPA_META_VideoCrop metadata to get the frame size. KDE and GNOME do
  // handle screen/window sharing differently. KDE/KWin doesn't use
  // SPA_META_VideoCrop metadata and when sharing a window, it always sets
  // stream size to size of the window. With that we just allocate the
  // DesktopFrame using the size of the stream itself. GNOME/Mutter
  // always sets stream size to the size of the whole screen, even when sharing
  // a window. To get the real window size we have to use SPA_META_VideoCrop
  // metadata. This gives us the size we need in order to allocate the
  // DesktopFrame.

  struct spa_meta_region* videocrop_metadata =
      static_cast<struct spa_meta_region*>(spa_buffer_find_meta_data(
          spa_buffer, SPA_META_VideoCrop, sizeof(*videocrop_metadata)));

  // Video size from metadata is bigger than an actual video stream size.
  // The metadata are wrong or we should up-scale the video...in both cases
  // just quit now.
  if (videocrop_metadata &&
      (videocrop_metadata->region.size.width >
           static_cast<uint32_t>(cb_data->stream.stream_size_.width()) ||
       videocrop_metadata->region.size.height >
           static_cast<uint32_t>(cb_data->stream.stream_size_.height()))) {
    RTC_LOG(LS_ERROR) << "Stream metadata sizes are wrong!";
    return;
  }

  // Use SPA_META_VideoCrop metadata to get the DesktopFrame size in case
  // a windows is shared and it represents just a small portion of the
  // stream itself. This will be for example used in case of GNOME (Mutter)
  // where the stream will have the size of the screen itself, but we care
  // only about smaller portion representing the window inside.
  bool videocrop_metadata_use = false;
  const struct spa_rectangle* videocrop_metadata_size =
      videocrop_metadata ? &videocrop_metadata->region.size : nullptr;

  if (videocrop_metadata_size && videocrop_metadata_size->width != 0 &&
      videocrop_metadata_size->height != 0 &&
      (static_cast<int>(videocrop_metadata_size->width) <
           cb_data->stream.stream_size_.width() ||
       static_cast<int>(videocrop_metadata_size->height) <
           cb_data->stream.stream_size_.height())) {
    videocrop_metadata_use = true;
  }

  if (videocrop_metadata_use) {
    cb_data->stream.frame_size_ = DesktopSize(videocrop_metadata_size->width,
                                              videocrop_metadata_size->height);
  } else {
    cb_data->stream.frame_size_ = cb_data->stream.stream_size_;
  }
  // Get the position of the video crop within the stream. Just double-check
  // that the position doesn't exceed the size of the stream itself. NOTE:
  // Currently it looks there is no implementation using this.
  uint32_t y_offset =
      videocrop_metadata_use && (videocrop_metadata->region.position.y +
                                 cb_data->stream.frame_size_.height() <=
                             cb_data->stream.stream_size_.height())
          ? videocrop_metadata->region.position.y
          : 0;
  uint32_t x_offset =
      videocrop_metadata_use && (videocrop_metadata->region.position.x +
                                 cb_data->stream.frame_size_.width() <=
                             cb_data->stream.stream_size_.width())
          ? videocrop_metadata->region.position.x
          : 0;

  const uint32_t stream_stride = kBytesPerPixel * cb_data->stream.stream_size_.width();
  uint32_t buffer_stride = spa_buffer->datas[0].chunk->stride;
  uint32_t src_stride = buffer_stride;

  if (spa_buffer->datas[0].type == SPA_DATA_DmaBuf &&
      buffer_stride > stream_stride) {
    // When DMA-BUFs are used, sometimes spa_buffer->stride we get might
    // contain additional padding, but after we import the buffer, the stride
    // we used is no longer relevant and we should just calculate it based on
    // the stream width. For more context see https://crbug.com/1333304.
    src_stride = stream_stride;
  }

  uint8_t* updated_src =
      src + (src_stride * y_offset) + (kBytesPerPixel * x_offset);

  webrtc::MutexLock lock(cb_data->stream.queue_lock_.get());

  auto& queue_ = cb_data->stream.queue_;
  // Move to the next frame if the current one is being used and shared
  if (queue_->current_frame() && queue_->current_frame()->IsShared()) {
    queue_->MoveToNextFrame();
    if (queue_->current_frame() && queue_->current_frame()->IsShared()) {
      RTC_LOG(LS_WARNING)
          << "Failed to process PipeWire buffer: no available frame";
      return;
    }
  }

  const auto& frame_size_ = cb_data->stream.frame_size_;
  if (!queue_->current_frame() ||
      !queue_->current_frame()->size().equals(frame_size_)) {
    std::unique_ptr<DesktopFrame> frame(new BasicDesktopFrame(
        DesktopSize(frame_size_.width(), frame_size_.height())));
    queue_->ReplaceCurrentFrame(SharedDesktopFrame::Wrap(std::move(frame)));
  }

  queue_->current_frame()->CopyPixelsFrom(
      updated_src, (src_stride - (kBytesPerPixel * x_offset)),
      DesktopRect::MakeWH(frame_size_.width(), frame_size_.height()));

  if (cb_data->stream.spa_video_format_.format == SPA_VIDEO_FORMAT_RGBx ||
      cb_data->stream.spa_video_format_.format == SPA_VIDEO_FORMAT_RGBA) {
    uint8_t* tmp_src = queue_->current_frame()->data();
    for (int i = 0; i < frame_size_.height(); ++i) {
      // If both sides decided to go with the RGBx format we need to convert
      // it to BGRx to match color format expected by WebRTC.
      ConvertRGBxToBGRx(tmp_src, queue_->current_frame()->stride());
      tmp_src += queue_->current_frame()->stride();
    }
  }

  queue_->current_frame()->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(queue_->current_frame()->size()));
}

}  // namespace

class SharedScreenCastStreamPrivate {
 public:
  SharedScreenCastStreamPrivate();
  ~SharedScreenCastStreamPrivate();

  // Creates a new stream for the given `stream_node_id` if one doesn't exist
  // already.
  bool StartScreenCastStream(uint32_t stream_node_id,
                             int fd,
                             uint32_t width = 0,
                             uint32_t height = 0);
  void UpdateScreenCastStreamResolution(uint32_t width, uint32_t height);
  void UpdateScreenCastStreamResolution(uint32_t width, uint32_t height,
                                        uint32_t node_id);
  void StopScreenCastStream();
  std::unique_ptr<DesktopFrame> CaptureFrame(uint32_t node_id);
  std::unique_ptr<MouseCursor> CaptureCursor();
  DesktopVector CaptureCursorPosition();

 private:
  std::unordered_map</*pw_stream_node_id=*/uint32_t, CallbackData>
      node_id_stream_;

  std::unique_ptr<EglDmaBuf> egl_dmabuf_;

  // PipeWire types
  struct pw_context* pw_context_ = nullptr;
  struct pw_core* pw_core_ = nullptr;
  struct pw_thread_loop* pw_main_loop_ = nullptr;

  spa_hook spa_core_listener_;
  // A number used to verify all previous methods and the resulting
  // events have been handled.
  int server_version_sync_ = 0;
  // Version of the running PipeWire server we communicate with
  PipeWireVersion pw_server_version_;
  bool retreived_server_version_ = false;
  // Version of the library used to run our code
  PipeWireVersion pw_client_version_;

  // event handlers
  pw_core_events pw_core_events_ = {};
  pw_stream_events pw_stream_events_ = {};

  // PipeWire callbacks
  static void OnCoreError(void* data,
                          uint32_t id,
                          int seq,
                          int res,
                          const char* message);
  static void OnCoreDone(void* user_data, uint32_t id, int seq);
  static void OnCoreInfo(void* user_data, const pw_core_info* info);
  static void OnStreamParamChanged(void* data,
                                   uint32_t id,
                                   const struct spa_pod* format);
  static void OnStreamStateChanged(void* data,
                                   pw_stream_state old_state,
                                   pw_stream_state state,
                                   const char* error_message);
  static void OnStreamProcess(void* data);
  // This will be invoked in case we fail to process DMA-BUF PW buffer using
  // negotiated stream parameters (modifier). We will drop the modifier we
  // failed to use and try to use a different one or fallback to shared memory
  // buffers.
  static void OnRenegotiateFormat(void* data, uint64_t);
};

void SharedScreenCastStreamPrivate::OnCoreError(void* data,
                                                uint32_t id,
                                                int seq,
                                                int res,
                                                const char* message) {
  SharedScreenCastStreamPrivate* that =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire remote error: " << message;
  if (message && std::string(message) == "connection error") {
    // TODO: Use a common cleanup method if this works.
    if (that->pw_main_loop_) {
      pw_thread_loop_stop(that->pw_main_loop_);
    }
    // TODO: See if we can somehow get an identifier of what stream to stop
    // here.
    for (auto& [_, cb_data] : that->node_id_stream_) {
      pw_stream_destroy(cb_data.stream.pw_stream_);
    }

    if (that->pw_context_) {
      pw_context_destroy(that->pw_context_);
      that->pw_context_ = nullptr;
    }

    if (that->pw_main_loop_) {
      pw_thread_loop_destroy(that->pw_main_loop_);
      that->pw_main_loop_ = nullptr;
    }

    that->node_id_stream_.clear();
  }
  // that->node_id_stream_.clear();
}

void SharedScreenCastStreamPrivate::OnCoreInfo(void* data,
                                               const pw_core_info* info) {
  SharedScreenCastStreamPrivate* stream =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(stream);

  stream->pw_server_version_ = PipeWireVersion::Parse(info->version);
  stream->retreived_server_version_ = true;
}

void SharedScreenCastStreamPrivate::OnCoreDone(void* data,
                                               uint32_t id,
                                               int seq) {
  const SharedScreenCastStreamPrivate* stream =
      static_cast<SharedScreenCastStreamPrivate*>(data);
  RTC_DCHECK(stream);

  if (id == PW_ID_CORE && stream->server_version_sync_ == seq) {
    pw_thread_loop_signal(stream->pw_main_loop_, false);
  }
}

// static
void SharedScreenCastStreamPrivate::OnStreamStateChanged(
    void* data,
    pw_stream_state old_state,
    pw_stream_state state,
    const char* error_message) {
  SharedScreenCastStreamPrivate* that =
      static_cast<SharedScreenCastStreamPrivate*>(data);
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
void SharedScreenCastStreamPrivate::OnStreamParamChanged(
    void* data,
    uint32_t id,
    const struct spa_pod* format) {
  CallbackData* that = static_cast<CallbackData*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire stream format changed.";
  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  spa_format_video_raw_parse(format, &that->stream.spa_video_format_);

  auto width = that->stream.spa_video_format_.size.width;
  auto height = that->stream.spa_video_format_.size.height;
  auto stride = SPA_ROUND_UP_N(width * kBytesPerPixel, 4);
  auto size = height * stride;

  that->stream.stream_size_ = DesktopSize(width, height);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.

  // When SPA_FORMAT_VIDEO_modifier is present we can use DMA-BUFs as
  // the server announces support for it.
  // See https://github.com/PipeWire/pipewire/blob/master/doc/dma-buf.dox
  const bool has_modifier =
      spa_pod_find_prop(format, nullptr, SPA_FORMAT_VIDEO_modifier);
  that->stream.modifier_ = has_modifier
                               ? that->stream.spa_video_format_.modifier
                               : DRM_FORMAT_MOD_INVALID;
  std::vector<const spa_pod*> params;
  const int buffer_types =
      has_modifier || (that->global.pw_server_version_ >= kDmaBufMinVersion)
          ? (1 << SPA_DATA_DmaBuf) | (1 << SPA_DATA_MemFd) |
                (1 << SPA_DATA_MemPtr)
          : (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);

  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_size, SPA_POD_Int(size), SPA_PARAM_BUFFERS_stride,
      SPA_POD_Int(stride), SPA_PARAM_BUFFERS_buffers,
      SPA_POD_CHOICE_RANGE_Int(8, 1, 32), SPA_PARAM_BUFFERS_dataType,
      SPA_POD_CHOICE_FLAGS_Int(buffer_types))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header)))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_VideoCrop), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_region)))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Cursor), SPA_PARAM_META_size,
      SPA_POD_CHOICE_RANGE_Int(CursorMetaSize(64, 64), CursorMetaSize(1, 1),
                               CursorMetaSize(384, 384)))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_VideoDamage), SPA_PARAM_META_size,
      SPA_POD_CHOICE_RANGE_Int(sizeof(struct spa_meta_region) * 16,
                               sizeof(struct spa_meta_region) * 1,
                               sizeof(struct spa_meta_region) * 16))));

  pw_stream_update_params(that->stream.pw_stream_, params.data(),
                          params.size());
}

// static
void SharedScreenCastStreamPrivate::OnStreamProcess(void* data) {
  CallbackData* that = static_cast<CallbackData*>(data);
  RTC_DCHECK(that);

  struct pw_buffer* next_buffer;
  struct pw_buffer* buffer = nullptr;

  next_buffer = pw_stream_dequeue_buffer(that->stream.pw_stream_);
  while (next_buffer) {
    buffer = next_buffer;
    next_buffer = pw_stream_dequeue_buffer(that->stream.pw_stream_);

    if (next_buffer) {
      pw_stream_queue_buffer(that->stream.pw_stream_, buffer);
    }
  }

  if (!buffer) {
    return;
  }

  ProcessBuffer(buffer, that);

  pw_stream_queue_buffer(that->stream.pw_stream_, buffer);
}

void SharedScreenCastStreamPrivate::OnRenegotiateFormat(void* data, uint64_t) {
  CallbackData* that = static_cast<CallbackData*>(data);
  RTC_DCHECK(that);

  {
    PipeWireThreadLoopLock thread_loop_lock(that->global.pw_main_loop_);

    uint8_t buffer[2048] = {};

    spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

    std::vector<const spa_pod*> params;
    struct spa_rectangle resolution =
        SPA_RECTANGLE(that->stream.width_, that->stream.height_);

    webrtc::MutexLock lock(that->stream.resolution_lock_.get());
    for (uint32_t format : {SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA,
                            SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx}) {
      if (!that->stream.modifiers_.empty()) {
        params.push_back(BuildFormat(
            &builder, format, that->stream.modifiers_,
            that->stream.pending_resolution_change_ ? &resolution : nullptr));
      }
      params.push_back(BuildFormat(
          &builder, format, /*modifiers=*/{},
          that->stream.pending_resolution_change_ ? &resolution : nullptr));
    }

    pw_stream_update_params(that->stream.pw_stream_, params.data(),
                            params.size());
    that->stream.pending_resolution_change_ = false;
  }
}

SharedScreenCastStreamPrivate::SharedScreenCastStreamPrivate() {
  // TODO: Make this more dynamic. Currently done to see if the map recreation
  // is causing invalidation of existing pointers/iterators.
  // 2 buckets didn't seem to help.
  // node_id_stream_.reserve(10);
}

SharedScreenCastStreamPrivate::~SharedScreenCastStreamPrivate() {
  RTC_LOG(LS_ERROR) << ">>> Destroying shared screen cast stream private";
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  // TODO: See if we can somehow get an identifier of what stream to stop here.
  for (auto& [_, cb_data] : node_id_stream_) {
    pw_stream_destroy(cb_data.stream.pw_stream_);
  }
  // if (pw_stream_) {
  //   pw_stream_destroy(pw_stream_);
  // }

  if (pw_core_) {
    pw_core_disconnect(pw_core_);
    pw_core_ = nullptr;
  }

  if (pw_context_) {
    pw_context_destroy(pw_context_);
    pw_context_ = nullptr;
  }

  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
    pw_main_loop_ = nullptr;
  }
}

RTC_NO_SANITIZE("cfi-icall")
bool SharedScreenCastStreamPrivate::StartScreenCastStream(
    uint32_t stream_node_id,
    int fd,
    uint32_t width,
    uint32_t height) {
  auto stream_it = node_id_stream_.find(stream_node_id);
  if (stream_it != node_id_stream_.end()) {
    RTC_LOG(LS_WARNING) << "Stream already exists for stream node id: "
                        << stream_node_id << ", reconnecting it";
    // pw_stream_disconnect(stream_it->second.stream.pw_stream_);
    // pw_stream_destroy(stream_it->second.stream.pw_stream_);
    node_id_stream_.erase(stream_it);
  }

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
  StubPathMap paths;

  // Check if the PipeWire and DRM libraries are available.
  paths[kModulePipewire].push_back(kPipeWireLib);
  paths[kModuleDrm].push_back(kDrmLib);

  if (!InitializeStubs(paths)) {
    RTC_LOG(LS_ERROR)
        << "One of following libraries is missing on your system:\n"
        << " - PipeWire (" << kPipeWireLib << ")\n"
        << " - drm (" << kDrmLib << ")";
    return false;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)
  if (!egl_dmabuf_)
    egl_dmabuf_ = std::make_unique<EglDmaBuf>();

  // Start the main loop only once regardless of how many streams are started.
  if (!pw_main_loop_) {
    RTC_DCHECK(!pw_context_);

    pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

    pw_main_loop_ = pw_thread_loop_new("pipewire-main-loop", nullptr);

    pw_context_ =
        pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
    if (!pw_context_) {
      RTC_LOG(LS_ERROR) << "Failed to create PipeWire context";
      return false;
    }

    if (pw_thread_loop_start(pw_main_loop_) < 0) {
      RTC_LOG(LS_ERROR) << "Failed to start main PipeWire loop";
      return false;
    }
    RTC_LOG(LS_ERROR) << ">>> Started pw thread loop";

    pw_client_version_ = PipeWireVersion::Parse(pw_get_library_version());

    // Initialize event handlers, remote end and stream-related.
    pw_core_events_.version = PW_VERSION_CORE_EVENTS;
    pw_core_events_.info = &OnCoreInfo;
    pw_core_events_.done = &OnCoreDone;
    pw_core_events_.error = &OnCoreError;

    PipeWireThreadLoopLock thread_loop_lock(pw_main_loop_);

    if (fd >= 0) {
      pw_core_ = pw_context_connect_fd(
          pw_context_, fcntl(fd, F_DUPFD_CLOEXEC), nullptr, 0);
    } else {
      pw_core_ = pw_context_connect(pw_context_, nullptr, 0);
    }

    if (!pw_core_) {
      RTC_LOG(LS_ERROR) << "Failed to connect PipeWire context";
      return false;
    }
    RTC_LOG(LS_ERROR) << ">>> Created pw core";

    pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

    RTC_LOG(LS_ERROR) << ">>> pw thread loop wait on version: "
                      << server_version_sync_;
    server_version_sync_ =
        pw_core_sync(pw_core_, PW_ID_CORE, server_version_sync_);

    pw_thread_loop_wait(pw_main_loop_);
    RTC_LOG(LS_ERROR) << ">>> pw thread loop wait done version: "
                      << server_version_sync_;
  }
  // }

  RTC_CHECK(retreived_server_version_);
  auto [cb_data_it, inserted] = node_id_stream_.emplace(
      stream_node_id,
      CallbackData{GlobalData{
                       .egl_dmabuf_ = egl_dmabuf_.get(),
                       .pw_server_version_ = pw_server_version_,
                       .pw_main_loop_ = pw_main_loop_,
                   },
                   StreamData{
                       .width_ = width,
                       .height_ = height,
                       .pw_stream_node_id_ = stream_node_id,
                   }});
  RTC_CHECK(inserted);

  {
    PipeWireThreadLoopLock thread_loop_lock(pw_main_loop_);
    CallbackData& cb_data = cb_data_it->second;
    // Add an event that can be later invoked by pw_loop_signal_event()
    cb_data.stream.renegotiate_ = pw_loop_add_event(
        pw_thread_loop_get_loop(pw_main_loop_), OnRenegotiateFormat, &cb_data);
    RTC_DCHECK(cb_data.stream.renegotiate_);
    pw_properties* reuseProps =
        pw_properties_new_string("pipewire.client.reuse=1");
    cb_data.stream.pw_stream_ =
        pw_stream_new(pw_core_, "webrtc-consume-stream", reuseProps);

    if (!cb_data.stream.pw_stream_) {
      RTC_LOG(LS_ERROR) << "Failed to create PipeWire stream";
      return false;
    }
    pw_stream_events_ =
        {
            .version = PW_VERSION_STREAM_EVENTS,
            .state_changed = &OnStreamStateChanged,
            .param_changed = &OnStreamParamChanged,
            .process = &OnStreamProcess,
        };
    pw_stream_add_listener(cb_data.stream.pw_stream_,
                           &cb_data.stream.spa_stream_listener_,
                           &pw_stream_events_, &cb_data);
    uint8_t buffer[2048] = {};

    spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

    std::vector<const spa_pod*> params;
    const bool has_required_pw_client_version =
        pw_client_version_ >= kDmaBufModifierMinVersion;
    const bool has_required_pw_server_version =
        pw_server_version_ >= kDmaBufModifierMinVersion;
    struct spa_rectangle resolution;
    bool set_resolution = false;
    if (width && height) {
      resolution = SPA_RECTANGLE(width, height);
      set_resolution = true;
    }
    for (uint32_t format : {SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA,
                            SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx}) {
      // Modifiers can be used with PipeWire >= 0.3.33
      if (has_required_pw_client_version && has_required_pw_server_version) {
        cb_data.stream.modifiers_ = egl_dmabuf_->QueryDmaBufModifiers(format);

        if (!cb_data.stream.modifiers_.empty()) {
          params.push_back(BuildFormat(&builder, format,
                                       cb_data.stream.modifiers_,
                                       set_resolution ? &resolution : nullptr));
        }
      }

      params.push_back(BuildFormat(&builder, format, /*modifiers=*/{},
                                   set_resolution ? &resolution : nullptr));
    }

    if (pw_stream_connect(cb_data.stream.pw_stream_, PW_DIRECTION_INPUT,
                          cb_data.stream.pw_stream_node_id_,
                          PW_STREAM_FLAG_AUTOCONNECT, params.data(),
                          params.size()) != 0) {
      RTC_LOG(LS_ERROR) << "Could not connect receiving stream with stream "
                        << "node id: " << cb_data.stream.pw_stream_node_id_;
      return false;
    }

    RTC_LOG(LS_ERROR) << ">>> PipeWire remote opened for stream node id: "
                      << cb_data.stream.pw_stream_node_id_;
  }
  return true;
}

RTC_NO_SANITIZE("cfi-icall")
void SharedScreenCastStreamPrivate::UpdateScreenCastStreamResolution(
    uint32_t width,
    uint32_t height) {
  if (!width || !height) {
    RTC_LOG(LS_WARNING) << "Bad resolution specified: " << width << "x"
                        << height;
    return;
  }
  if (!pw_main_loop_) {
    RTC_LOG(LS_WARNING) << "No main pipewire loop, ignoring resolution change";
    return;
  }
  // TODO: Figure out the exact stream for which we are updating the
  // resolution.
  for (auto& [node_id, cb_data] : node_id_stream_) {
    auto& stream = cb_data.stream;
    if (!stream.renegotiate_) {
      RTC_LOG(LS_WARNING) << "Can not renegotiate stream params, ignoring "
                          << "resolution change";
      return;
    }
    if (stream.width_ != width || stream.height_ != height) {
      RTC_LOG(LS_WARNING) << ">>> Updating resolution of pipwire node: " <<
        node_id << " from: " << stream.width_ << "x" << stream.height_
        << " to: " << width << "x" << height;
      stream.width_ = width;
      stream.height_ = height;
      {
        webrtc::MutexLock lock(stream.resolution_lock_.get());
        stream.pending_resolution_change_ = true;
      }
      // TODO: Fix unknown race/bug in pipewire misses stream params updates
      // if back to back updates are made very fast.
      pw_loop_signal_event(pw_thread_loop_get_loop(cb_data.global.pw_main_loop_),
                           stream.renegotiate_);
      // TODO: This is a workaround of (what looks like) a pipewire bug.
      // (Neither locking on the main pipewireloop or waiting on
      // `stream.pending_resolution_change_` works here)
      rtc::Thread::SleepMs(1000);
    }
  }
}

RTC_NO_SANITIZE("cfi-icall")
void SharedScreenCastStreamPrivate::UpdateScreenCastStreamResolution(
    uint32_t width,
    uint32_t height, uint32_t node_id) {
  if (!width || !height) {
    RTC_LOG(LS_WARNING) << "Bad resolution specified: " << width << "x"
                        << height;
    return;
  }
  if (!pw_main_loop_) {
    RTC_LOG(LS_WARNING) << "No main pipewire loop, ignoring resolution change";
    return;
  }
  // TODO: Figure out the exact stream for which we are updating the
  // resolution.
  auto cb_data_it = node_id_stream_.find(node_id);
  if (cb_data_it == node_id_stream_.end()) {
    RTC_LOG(LS_WARNING) << ">>> Unable to find stream for node id: "
                        << node_id;
    return;
  }
  RTC_LOG(LS_WARNING) << ">>> Successfully found stream for node id: "
                      << node_id;
  auto& cb_data = cb_data_it->second;
  auto& stream = cb_data.stream;
  if (!stream.renegotiate_) {
    RTC_LOG(LS_WARNING) << "Can not renegotiate stream params, ignoring "
                        << "resolution change";
    return;
  }
  if (stream.width_ != width || stream.height_ != height) {
    RTC_LOG(LS_WARNING) << ">>> Updating resolution of individual pipwire "
                        << "node: " << node_id << " from: " << stream.width_
                        << "x" << stream.height_ << " to: " << width << "x"
                        << height;
    stream.width_ = width;
    stream.height_ = height;
    {
      webrtc::MutexLock lock(stream.resolution_lock_.get());
      stream.pending_resolution_change_ = true;
    }
    // TODO: Fix unknown race/bug in pipewire misses stream params updates
    // if back to back updates are made very fast.
    pw_loop_signal_event(pw_thread_loop_get_loop(cb_data.global.pw_main_loop_),
                         stream.renegotiate_);
    rtc::Thread::SleepMs(1000);
  }
}


void SharedScreenCastStreamPrivate::StopScreenCastStream() {
  // TODO: Fix this.
  RTC_LOG(LS_ERROR) << __func__ << " : >>> Stopping all screencast streams";
  //  for (auto& [_, cb_data] : node_id_stream_) {
  //    pw_stream_disconnect(cb_data.stream.pw_stream_);
  //  }
  // TODO: This might have implications on the stream shared with the mouse
  // cursor monitor.
  RTC_LOG(LS_ERROR) << __func__ << " : >>> Stopping main loop";
  if (pw_main_loop_) {
    pw_thread_loop_stop(pw_main_loop_);
  }

  RTC_LOG(LS_ERROR) << __func__ << " : >>> Destroying streams";
  // TODO: See if we can somehow get an identifier of what stream to stop here.
  for (auto& [_, cb_data] : node_id_stream_) {
    pw_stream_destroy(cb_data.stream.pw_stream_);
  }
  // if (pw_stream_) {
  //   pw_stream_destroy(pw_stream_);
  // }

  RTC_LOG(LS_ERROR) << __func__ << " : >>> Disconnecting core";
  if (pw_core_) {
    pw_core_disconnect(pw_core_);
    pw_core_ = nullptr;
  }

  RTC_LOG(LS_ERROR) << __func__ << " : >>> Disconnecting context";
  if (pw_context_) {
    pw_context_destroy(pw_context_);
    pw_context_ = nullptr;
  }

  RTC_LOG(LS_ERROR) << __func__ << " : >>> Destroying thread loop";
  if (pw_main_loop_) {
    pw_thread_loop_destroy(pw_main_loop_);
    pw_main_loop_ = nullptr;
  }

  node_id_stream_.clear();
}

std::unique_ptr<DesktopFrame> SharedScreenCastStreamPrivate::CaptureFrame(
    uint32_t node_id) {
  auto it = node_id_stream_.find(node_id);
  if (it == node_id_stream_.end()) {
    RTC_LOG(LS_ERROR) << "Can not capture frames from node id: " << node_id
                      << ", no corresponding stream found";
    return nullptr;
  }
  CallbackData& cb_data = it->second;
  webrtc::MutexLock lock(cb_data.stream.queue_lock_.get());

  if (!cb_data.stream.queue_->current_frame()) {
    return std::unique_ptr<DesktopFrame>{};
  }

  std::unique_ptr<SharedDesktopFrame> frame =
      cb_data.stream.queue_->current_frame()->Share();
  return std::move(frame);
}

std::unique_ptr<MouseCursor> SharedScreenCastStreamPrivate::CaptureCursor() {
  for (auto& [id, cb_data] : node_id_stream_) {
    if (cb_data.stream.mouse_cursor_)
      return std::move(cb_data.stream.mouse_cursor_);
  }
  return nullptr;
}

DesktopVector SharedScreenCastStreamPrivate::CaptureCursorPosition() {
  // TODO: This needs fixing.
  if (node_id_stream_.empty())
    return DesktopVector(-1, -1);
  return node_id_stream_.begin()->second.stream.mouse_cursor_position_;
}

SharedScreenCastStream::SharedScreenCastStream()
    : private_(std::make_unique<SharedScreenCastStreamPrivate>()) {}

SharedScreenCastStream::~SharedScreenCastStream() {
  RTC_LOG(LS_ERROR) << ">>> Destroying SharedScreenCastStream";
}

rtc::scoped_refptr<SharedScreenCastStream>
SharedScreenCastStream::CreateDefault() {
  // Explicit new, to access non-public constructor.
  return rtc::scoped_refptr(new SharedScreenCastStream());
}

bool SharedScreenCastStream::StartScreenCastStream(uint32_t stream_node_id) {
  return private_->StartScreenCastStream(stream_node_id, -1);
}

bool SharedScreenCastStream::StartScreenCastStream(uint32_t stream_node_id,
                                                   int fd,
                                                   uint32_t width,
                                                   uint32_t height) {
  return private_->StartScreenCastStream(stream_node_id, fd, width, height);
}

void SharedScreenCastStream::UpdateScreenCastStreamResolution(uint32_t width,
                                                              uint32_t height) {
  private_->UpdateScreenCastStreamResolution(width, height);
}

void SharedScreenCastStream::UpdateScreenCastStreamResolution(uint32_t width,
                                                              uint32_t height,
                                                              uint32_t node_id) {
  private_->UpdateScreenCastStreamResolution(width, height, node_id);
}


void SharedScreenCastStream::StopScreenCastStream() {
  RTC_LOG(LS_INFO) << __func__ << " : >>> Stopping all screencast streams";
  private_->StopScreenCastStream();
}

std::unique_ptr<DesktopFrame> SharedScreenCastStream::CaptureFrame(
    uint32_t node_id) {
  return private_->CaptureFrame(node_id);
}

std::unique_ptr<MouseCursor> SharedScreenCastStream::CaptureCursor() {
  // TODO: Would need a mechanism to determine which `pw_stream_node_id` stream
  // is active and return cursor from that stream.
  return private_->CaptureCursor();
}

absl::optional<DesktopVector> SharedScreenCastStream::CaptureCursorPosition() {
  DesktopVector position = private_->CaptureCursorPosition();

  // Consider only (x >= 0 and y >= 0) a valid position
  if (position.x() < 0 || position.y() < 0) {
    return absl::nullopt;
  }

  return position;
}

}  // namespace webrtc
