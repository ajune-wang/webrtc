/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/screen_capturer_fuchsia.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "modules/desktop_capture/blank_detector_desktop_capturer_wrapper.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/fallback_desktop_capturer_wrapper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<ScreenCapturerFuchsia> capturer(new ScreenCapturerFuchsia());
  return capturer;
}

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

ScreenCapturerFuchsia::ScreenCapturerFuchsia()
    : component_context_(
          sys::ComponentContext::CreateAndServeOutgoingDirectory()) {}

void ScreenCapturerFuchsia::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);
  callback_ = callback;

  fatal_error_ = false;

  SetupBuffers();
}

void ScreenCapturerFuchsia::CaptureFrame() {
  if (fatal_error_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  int64_t capture_start_time_nanos = rtc::TimeNanos();

  zx::event event;
  zx::event dup;
  zx_status_t status = zx::event::create(0, &event);
  if (status != ZX_OK) {
    RTC_LOG(LS_ERROR) << "Failed to create event: " << status;
    return;
  }
  event.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup);

  fuchsia::ui::composition::GetNextFrameArgs gnf_args;
  gnf_args.set_event(std::move(dup));

  fuchsia::ui::composition::ScreenCapture_GetNextFrame_Result result;
  screen_capture_->GetNextFrame(std::move(gnf_args), &result);
  if (result.is_err()) {
    RTC_LOG(LS_ERROR) << "fuchsia.ui.composition.GetNextFrame() failed: "
                      << result.err() << "\n";
    return;
  }

  status = event.wait_one(ZX_EVENT_SIGNALED, zx::deadline_after(kEventDelay),
                          nullptr);
  if (status != ZX_OK) {
    RTC_LOG(LS_ERROR) << "Timed out waiting for ScreenCapture to render frame: "
                      << status;
    return;
  }
  uint32_t buffer_id = result.response().buffer_id();

  std::unique_ptr<BasicDesktopFrame> frame(
      new BasicDesktopFrame(DesktopSize(width_, height_)));

  status = buffer_collection_info_.buffers[buffer_id].vmo.op_range(
      ZX_CACHE_FLUSH_DATA | ZX_VMO_OP_CACHE_INVALIDATE, 0,
      buffer_collection_info_.settings.buffer_settings.size_bytes, nullptr, 0);
  if (status != ZX_OK) {
    RTC_LOG(LS_ERROR) << "Failed to flush vmo cache: " << status;
  }
  uint32_t pixels_per_row = GetPixelsPerRow(
      buffer_collection_info_.settings.image_format_constraints);
  MapHostPointer(buffer_collection_info_, buffer_id,
                 [this, &frame, pixels_per_row, buffer_id](uint8_t* vmo_host,
                                                           uint32_t num_bytes) {
                   uint32_t stride = /*kBytesPerPixel=*/4 * pixels_per_row;
                   frame->CopyPixelsFrom(vmo_host, stride,
                                         DesktopRect::MakeWH(width_, height_));
                   fuchsia::ui::composition::ScreenCapture_ReleaseFrame_Result
                       release_result;
                   screen_capture_->ReleaseFrame(buffer_id, &release_result);
                   if (release_result.is_err()) {
                     RTC_LOG(LS_ERROR)
                         << "fuchsia.ui.composition.ReleaseFrame() failed: "
                         << release_result.err();
                   }
                 });

  int capture_time_ms = (rtc::TimeNanos() - capture_start_time_nanos) /
                        rtc::kNumNanosecsPerMillisec;
  frame->set_capture_time_ms(capture_time_ms);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

bool ScreenCapturerFuchsia::GetSourceList(SourceList* screens) {
  RTC_DCHECK(screens->size() == 0);
  // Fuchsia only supports single monitor display at this point
  screens->push_back({kFuchsiaScreenId, std::string("Fuchsia monitor")});
  return true;
}

bool ScreenCapturerFuchsia::SelectSource(SourceId id) {
  if (id == kFuchsiaScreenId || id == kFullDesktopScreenId) {
    return true;
  }
  return false;
}

fuchsia::sysmem::BufferCollectionConstraints
ScreenCapturerFuchsia::GetBufferConstraints() {
  fuchsia::sysmem::BufferCollectionConstraints constraints;
  // TODO(smpham): Figure out what these constraints should actually be
  constraints.usage.cpu =
      fuchsia::sysmem::cpuUsageRead | fuchsia::sysmem::cpuUsageWrite;
  constraints.min_buffer_count = kMinBufferCount;

  constraints.has_buffer_memory_constraints = true;
  constraints.buffer_memory_constraints.ram_domain_supported = true;
  constraints.buffer_memory_constraints.cpu_domain_supported = true;

  constraints.image_format_constraints_count = 1;
  fuchsia::sysmem::ImageFormatConstraints& image_constraints =
      constraints.image_format_constraints[0];
  image_constraints.color_spaces_count = 1;
  image_constraints.color_space[0] = fuchsia::sysmem::ColorSpace{
      .type = fuchsia::sysmem::ColorSpaceType::SRGB};
  image_constraints.pixel_format.type =
      fuchsia::sysmem::PixelFormatType::BGRA32;
  image_constraints.pixel_format.has_format_modifier = true;
  image_constraints.pixel_format.format_modifier.value =
      fuchsia::sysmem::FORMAT_MODIFIER_LINEAR;

  image_constraints.required_min_coded_width = width_;
  image_constraints.required_min_coded_height = height_;
  image_constraints.required_max_coded_width = width_;
  image_constraints.required_max_coded_height = height_;

  image_constraints.bytes_per_row_divisor = kBytesPerPixel;

  return constraints;
}

void ScreenCapturerFuchsia::SetupBuffers() {
  // set up buffers
  fuchsia::ui::scenic::ScenicSyncPtr scenic;
  zx_status_t status = component_context_->svc()->Connect(scenic.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to connect to Scenic: " << status;
    return;
  }

  bool scenic_uses_flatland = false;
  scenic->UsesFlatland(&scenic_uses_flatland);
  if (!scenic_uses_flatland) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Screen capture not supported without Flatland.";
    return;
  }

  fuchsia::ui::gfx::DisplayInfo display_info;
  status = scenic->GetDisplayInfo(&display_info);
  width_ = display_info.width_in_px;
  height_ = display_info.height_in_px;

  status = component_context_->svc()->Connect(sysmem_allocator_.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to connect to Sysmem Allocator: " << status;
    return;
  }

  fuchsia::sysmem::BufferCollectionTokenSyncPtr sysmem_token;
  status =
      sysmem_allocator_->AllocateSharedCollection(sysmem_token.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR)
        << "fuchsia.sysmem.Allocator.AllocateSharedCollection() failed: "
        << status;
    return;
  }

  fuchsia::sysmem::BufferCollectionTokenSyncPtr flatland_token;
  status = sysmem_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                                   flatland_token.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR)
        << "fuchsia.sysmem.BufferCollectionToken.Duplicate() failed: "
        << status;
    return;
  }

  status = sysmem_token->Sync();
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "fuchsia.sysmem.BufferCollectionToken.Sync() failed: "
                      << status;
    return;
  }

  status = sysmem_allocator_->BindSharedCollection(std::move(sysmem_token),
                                                   collection_.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR)
        << "fuchsia.sysmem.Allocator.BindSharedCollection() failed: " << status;
    return;
  }

  status = collection_->SetConstraints(/*has_constraints=*/true,
                                       GetBufferConstraints());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR)
        << "fuchsia.sysmem.BufferCollection.SetConstraints() failed: "
        << status;
    return;
  }

  fuchsia::ui::composition::BufferCollectionImportToken import_token;
  fuchsia::ui::composition::BufferCollectionExportToken export_token;
  status = zx::eventpair::create(0, &export_token.value, &import_token.value);
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR)
        << "Failed to create BufferCollection import and export tokens: "
        << status;
    return;
  }

  status = component_context_->svc()->Connect(flatland_allocator_.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to connect to Flatland Allocator: " << status;
    return;
  }

  fuchsia::ui::composition::RegisterBufferCollectionArgs rbc_args;
  rbc_args.set_export_token(std::move(export_token));
  rbc_args.set_buffer_collection_token(std::move(flatland_token));
  rbc_args.set_usage(
      fuchsia::ui::composition::RegisterBufferCollectionUsage::SCREENSHOT);

  fuchsia::ui::composition::Allocator_RegisterBufferCollection_Result
      rbc_result;
  flatland_allocator_->RegisterBufferCollection(std::move(rbc_args),
                                                &rbc_result);
  if (rbc_result.is_err()) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "fuchsia.ui.composition.Allocator."
                         "RegisterBufferCollection() failed.";
    return;
  }

  zx_status_t allocation_status;
  status = collection_->WaitForBuffersAllocated(&allocation_status,
                                                &buffer_collection_info_);
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to wait for buffer collection info: "
                      << status;
    return;
  }
  if (allocation_status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to allocate buffer collection: " << status;
    return;
  }
  status = collection_->Close();
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to close buffer collection token: " << status;
    return;
  }

  status = component_context_->svc()->Connect(screen_capture_.NewRequest());
  if (status != ZX_OK) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR) << "Failed to connect to Screen Capture: " << status;
    return;
  }

  // Configure buffers in ScreenCapture client.
  fuchsia::ui::composition::ScreenCaptureConfig sc_args;
  sc_args.set_import_token(std::move(import_token));
  sc_args.set_buffer_count(buffer_collection_info_.buffer_count);
  sc_args.set_size({width_, height_});

  fuchsia::ui::composition::ScreenCapture_Configure_Result sc_result;
  screen_capture_->Configure(std::move(sc_args), &sc_result);
  if (sc_result.is_err()) {
    fatal_error_ = true;
    RTC_LOG(LS_ERROR)
        << " fuchsia.ui.composition.ScreenCapture.Configure() failed: "
        << sc_result.err();
    return;
  }
}

uint32_t ScreenCapturerFuchsia::GetPixelsPerRow(
    const fuchsia::sysmem::ImageFormatConstraints& constraints) {
  uint32_t stride =
      RoundUp(std::max(constraints.min_bytes_per_row, width_ * kBytesPerPixel),
              constraints.bytes_per_row_divisor);
  uint32_t pixels_per_row = stride / kBytesPerPixel;

  return pixels_per_row;
}

void ScreenCapturerFuchsia::MapHostPointer(
    const fuchsia::sysmem::BufferCollectionInfo_2& collection_info,
    uint32_t vmo_idx,
    std::function<void(uint8_t*, uint32_t)> callback) {
  // If the vmo idx is out of bounds pass in a nullptr and 0 bytes back to the
  // caller.
  if (vmo_idx >= collection_info.buffer_count) {
    callback(nullptr, 0);
    return;
  }

  const zx::vmo& vmo = collection_info.buffers[vmo_idx].vmo;
  auto vmo_bytes = collection_info.settings.buffer_settings.size_bytes;
  RTC_DCHECK(vmo_bytes > 0);

  uint8_t* vmo_host = nullptr;
  auto status = zx::vmar::root_self()->map(
      ZX_VM_PERM_WRITE | ZX_VM_PERM_READ, /*vmar_offset*/ 0, vmo,
      /*vmo_offset*/ 0, vmo_bytes, reinterpret_cast<uintptr_t*>(&vmo_host));
  RTC_DCHECK(status == ZX_OK);
  callback(vmo_host, vmo_bytes);

  // Unmap the pointer.
  uintptr_t address = reinterpret_cast<uintptr_t>(vmo_host);
  status = zx::vmar::root_self()->unmap(address, vmo_bytes);
  RTC_DCHECK(status == ZX_OK);
}

}  // namespace webrtc
