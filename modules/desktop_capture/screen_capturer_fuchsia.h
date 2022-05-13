/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_FUCHSIA_H_
#define MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_FUCHSIA_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"

namespace webrtc {

class ScreenCapturerFuchsia final : public DesktopCapturer {
 public:
  ScreenCapturerFuchsia();

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* screens) override;
  bool SelectSource(SourceId id) override;

 private:
  // What is reasonable here? Do we even need multiple?
  static constexpr uint32_t kMinBufferCount = 2;
  static constexpr uint32_t kBytesPerPixel = 4;
  static constexpr DesktopCapturer::SourceId kFuchsiaScreenId = 1;
  static constexpr zx::duration kEventDelay = zx::msec(5000);

  fuchsia::sysmem::BufferCollectionConstraints GetBufferConstraints();
  void SetupBuffers();
  uint32_t GetPixelsPerRow(
      const fuchsia::sysmem::ImageFormatConstraints& constraints);
  void MapHostPointer(
      const fuchsia::sysmem::BufferCollectionInfo_2& collection_info,
      uint32_t vmo_idx,
      std::function<void(uint8_t*, uint32_t)> callback);

  Callback* callback_ = nullptr;

  std::unique_ptr<sys::ComponentContext> component_context_;
  fuchsia::sysmem::AllocatorSyncPtr sysmem_allocator_;
  fuchsia::ui::composition::AllocatorSyncPtr flatland_allocator_;
  fuchsia::ui::composition::ScreenCaptureSyncPtr screen_capture_;
  fuchsia::sysmem::BufferCollectionSyncPtr collection_;
  fuchsia::sysmem::BufferCollectionInfo_2 buffer_collection_info_;

  bool fatal_error_;

  uint32_t width_;
  uint32_t height_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_FUCHSIA_H_
