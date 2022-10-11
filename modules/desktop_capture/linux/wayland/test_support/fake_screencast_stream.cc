
/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/test_support/fake_screencast_stream.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "modules/desktop_capture/rgba_color.h"
#include "rtc_base/logging.h"

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
#include "modules/desktop_capture/linux/wayland/pipewire_stubs.h"
using modules_desktop_capture_linux_wayland::InitializeStubs;
using modules_desktop_capture_linux_wayland::kModuleDrm;
using modules_desktop_capture_linux_wayland::kModulePipewire;
using modules_desktop_capture_linux_wayland::StubPathMap;
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

namespace webrtc {

#if defined(WEBRTC_DLOPEN_PIPEWIRE)
const char kPipeWireLib[] = "libpipewire-0.3.so.0";
const char kDrmLib[] = "libdrm.so.2";
#endif

constexpr int kBytesPerPixel = 4;
constexpr int kWidth = 800;
constexpr int kHeight = 640;

FakeScreenCastStream::FakeScreenCastStream(StreamNotifier* notifier)
    : notifier_(notifier), random_generator_(100) {
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
    return;
  }
#endif  // defined(WEBRTC_DLOPEN_PIPEWIRE)

  pw_init(/*argc=*/nullptr, /*argc=*/nullptr);

  pw_main_loop_ = pw_thread_loop_new("pipewire-test-main-loop", nullptr);

  pw_context_ =
      pw_context_new(pw_thread_loop_get_loop(pw_main_loop_), nullptr, 0);
  if (!pw_context_) {
    RTC_LOG(LS_ERROR) << "PipeWire test: Failed to create PipeWire context";
    return;
  }

  if (pw_thread_loop_start(pw_main_loop_) < 0) {
    RTC_LOG(LS_ERROR) << "PipeWire test: Failed to start main PipeWire loop";
    return;
  }

  // Initialize event handlers, remote end and stream-related.
  pw_core_events_.version = PW_VERSION_CORE_EVENTS;
  pw_core_events_.error = &OnCoreError;

  pw_stream_events_.version = PW_VERSION_STREAM_EVENTS;
  pw_stream_events_.add_buffer = &OnStreamAddBuffer;
  pw_stream_events_.remove_buffer = &OnStreamRemoveBuffer;
  pw_stream_events_.state_changed = &OnStreamStateChanged;
  pw_stream_events_.param_changed = &OnStreamParamChanged;

  {
    PipeWireThreadLoopLock thread_loop_lock(pw_main_loop_);

    pw_core_ = pw_context_connect(pw_context_, nullptr, 0);
    if (!pw_core_) {
      RTC_LOG(LS_ERROR) << "PipeWire test: Failed to connect PipeWire context";
      return;
    }

    pw_core_add_listener(pw_core_, &spa_core_listener_, &pw_core_events_, this);

    pw_stream_ = pw_stream_new(pw_core_, "webrtc-test-stream", nullptr);

    if (!pw_stream_) {
      RTC_LOG(LS_ERROR) << "PipeWire test: Failed to create PipeWire stream";
      return;
    }

    pw_stream_add_listener(pw_stream_, &spa_stream_listener_,
                           &pw_stream_events_, this);
    uint8_t buffer[2048] = {};

    spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer)};

    std::vector<const spa_pod*> params;

    spa_rectangle resolution =
        SPA_RECTANGLE(uint32_t(kWidth), uint32_t(kHeight));
    params.push_back(BuildFormat(&builder, SPA_VIDEO_FORMAT_BGRx,
                                 /*modifiers=*/{}, &resolution));

    auto flags =
        pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_ALLOC_BUFFERS);
    if (pw_stream_connect(pw_stream_, PW_DIRECTION_OUTPUT, SPA_ID_INVALID,
                          flags, params.data(), params.size()) != 0) {
      RTC_LOG(LS_ERROR) << "PipeWire test: Could not connect receiving stream.";
      pw_stream_destroy(pw_stream_);
      pw_stream_ = nullptr;
      return;
    }
  }

  RTC_LOG(LS_ERROR) << "PipeWire test: PipeWire remote opened.";

  return;
}

FakeScreenCastStream::~FakeScreenCastStream() {
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

void FakeScreenCastStream::RecordFrame() {
  const char* error;
  if (pw_stream_get_state(pw_stream_, &error) != PW_STREAM_STATE_STREAMING) {
    if (error) {
      RTC_LOG(LS_ERROR)
          << "PipeWire test: Failed to record frame: stream is not active: "
          << error;
    }
  }

  struct pw_buffer* buffer = pw_stream_dequeue_buffer(pw_stream_);
  if (!buffer) {
    RTC_LOG(LS_ERROR) << "PipeWire test: No available buffer";
    return;
  }

  struct spa_buffer* spa_buffer = buffer->buffer;
  struct spa_data* spa_data = spa_buffer->datas;
  uint8_t* data = static_cast<uint8_t*>(spa_data->data);
  if (!data) {
    RTC_LOG(LS_ERROR)
        << "PipeWire test: Failed to record frame: invalid buffer data";
    pw_stream_queue_buffer(pw_stream_, buffer);
    return;
  }

  const int stride = SPA_ROUND_UP_N(kWidth * kBytesPerPixel, 4);

  spa_data->chunk->offset = 0;
  spa_data->chunk->size = kHeight * stride;
  spa_data->chunk->stride = stride;

  RgbaColor rgba_color(random_generator_.Rand(255), random_generator_.Rand(255),
                       random_generator_.Rand(255));
  uint32_t color = rgba_color.ToUInt32();
  for (int i = 0; i < kHeight; i++) {
    uint32_t* column = reinterpret_cast<uint32_t*>(data);
    for (int j = 0; j < kWidth; j++) {
      column[j] = color;
    }
    data += stride;
  }

  pw_stream_queue_buffer(pw_stream_, buffer);
  if (notifier_) {
    notifier_->OnFrameRecorded();
  }
}

void FakeScreenCastStream::StartStreaming() {
  if (pw_stream_ && pw_node_id_ != 0) {
    pw_stream_set_active(pw_stream_, true);
  }
}

void FakeScreenCastStream::StopStreaming() {
  if (pw_stream_ && pw_node_id_ != 0) {
    pw_stream_set_active(pw_stream_, false);
  }
}

// static
void FakeScreenCastStream::OnCoreError(void* data,
                                       uint32_t id,
                                       int seq,
                                       int res,
                                       const char* message) {
  FakeScreenCastStream* that = static_cast<FakeScreenCastStream*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_ERROR) << "PipeWire test: PipeWire remote error: " << message;
}

// static
void FakeScreenCastStream::OnStreamStateChanged(void* data,
                                                pw_stream_state old_state,
                                                pw_stream_state state,
                                                const char* error_message) {
  FakeScreenCastStream* that = static_cast<FakeScreenCastStream*>(data);
  RTC_DCHECK(that);

  switch (state) {
    case PW_STREAM_STATE_ERROR:
      RTC_LOG(LS_ERROR) << "PipeWire test: PipeWire stream state error: "
                        << error_message;
      break;
    case PW_STREAM_STATE_PAUSED:
      if (that->pw_node_id_ == 0 && that->pw_stream_) {
        that->pw_node_id_ = pw_stream_get_node_id(that->pw_stream_);
        that->notifier_->OnStreamReady(that->pw_node_id_);
      } else {
        // Stop streaming
        that->is_streaming_ = false;
        that->notifier_->OnStopStreaming();
      }
      break;
    case PW_STREAM_STATE_STREAMING:
      // Start streaming
      that->is_streaming_ = true;
      that->notifier_->OnStartStreaming();
      break;
    case PW_STREAM_STATE_CONNECTING:
      break;
    case PW_STREAM_STATE_UNCONNECTED:
      if (that->is_streaming_) {
        // Stop streaming
        that->is_streaming_ = false;
        that->notifier_->OnStopStreaming();
      }
      break;
  }
}

// static
void FakeScreenCastStream::OnStreamParamChanged(void* data,
                                                uint32_t id,
                                                const struct spa_pod* format) {
  FakeScreenCastStream* that = static_cast<FakeScreenCastStream*>(data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "PipeWire test: PipeWire stream format changed.";
  if (!format || id != SPA_PARAM_Format) {
    return;
  }

  spa_format_video_raw_parse(format, &that->spa_video_format_);

  auto stride = SPA_ROUND_UP_N(kWidth * kBytesPerPixel, 4);

  uint8_t buffer[1024] = {};
  auto builder = spa_pod_builder{buffer, sizeof(buffer)};

  // Setup buffers and meta header for new format.

  std::vector<const spa_pod*> params;
  const int buffer_types = (1 << SPA_DATA_MemFd);
  spa_rectangle resolution = SPA_RECTANGLE(uint32_t(kWidth), uint32_t(kHeight));

  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&resolution),
      SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(16, 2, 16),
      SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1), SPA_PARAM_BUFFERS_stride,
      SPA_POD_Int(stride), SPA_PARAM_BUFFERS_size,
      SPA_POD_Int(stride * kHeight), SPA_PARAM_BUFFERS_align, SPA_POD_Int(16),
      SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(buffer_types))));
  params.push_back(reinterpret_cast<spa_pod*>(spa_pod_builder_add_object(
      &builder, SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, SPA_PARAM_META_type,
      SPA_POD_Id(SPA_META_Header), SPA_PARAM_META_size,
      SPA_POD_Int(sizeof(struct spa_meta_header)))));

  pw_stream_update_params(that->pw_stream_, params.data(), params.size());
}

// static
void FakeScreenCastStream::OnStreamAddBuffer(void* data, pw_buffer* buffer) {
  FakeScreenCastStream* that = static_cast<FakeScreenCastStream*>(data);
  RTC_DCHECK(that);

  struct spa_data* spa_data = buffer->buffer->datas;

  spa_data->mapoffset = 0;
  spa_data->flags = SPA_DATA_FLAG_READWRITE;

  if (!(spa_data[0].type & (1 << SPA_DATA_MemFd))) {
    RTC_LOG(LS_ERROR)
        << "PipeWire test: Client doesn't support memfd buffer data type";
    return;
  }

  const int stride = SPA_ROUND_UP_N(kWidth * kBytesPerPixel, 4);
  spa_data->maxsize = stride * kHeight;
  spa_data->type = SPA_DATA_MemFd;
  spa_data->fd =
      memfd_create("pipewire-test-memfd", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (spa_data->fd == -1) {
    RTC_LOG(LS_ERROR) << "PipeWire test: Can't create memfd";
    return;
  }

  spa_data->mapoffset = 0;

  if (ftruncate(spa_data->fd, spa_data->maxsize) < 0) {
    RTC_LOG(LS_ERROR) << "PipeWire test: Can't truncate to"
                      << spa_data->maxsize;
    return;
  }

  unsigned int seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
  if (fcntl(spa_data->fd, F_ADD_SEALS, seals) == -1) {
    RTC_LOG(LS_ERROR) << "PipeWire test: Failed to add seals";
  }

  spa_data->data = mmap(nullptr, spa_data->maxsize, PROT_READ | PROT_WRITE,
                        MAP_SHARED, spa_data->fd, spa_data->mapoffset);
  if (spa_data->data == MAP_FAILED) {
    RTC_LOG(LS_ERROR) << "PipeWire test: Failed to mmap memory";
  } else {
    RTC_LOG(LS_INFO) << "PipeWire test: Memfd created successfully: "
                     << spa_data->data << spa_data->maxsize;
  }
}

// static
void FakeScreenCastStream::OnStreamRemoveBuffer(void* data, pw_buffer* buffer) {
  FakeScreenCastStream* that = static_cast<FakeScreenCastStream*>(data);
  RTC_DCHECK(that);

  struct spa_buffer* spa_buffer = buffer->buffer;
  struct spa_data* spa_data = spa_buffer->datas;
  if (spa_data && spa_data->type == SPA_DATA_MemFd) {
    munmap(spa_data->data, spa_data->maxsize);
    close(spa_data->fd);
  }
}

uint32_t FakeScreenCastStream::PipeWireNodeId() {
  return pw_node_id_;
}

}  // namespace webrtc
