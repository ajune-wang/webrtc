/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_and_cursor_composer.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include <d3d11_4.h>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/mouse_cursor_monitor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

#include "modules/desktop_capture/win/d3d_device.h"
#include "modules/desktop_capture/win/desktop_capture_utils.h"
#include "modules/desktop_capture/win/desktop_frame_texture.h"

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"


namespace webrtc {

namespace {

// Global reference counter which is increased when a DesktopFrameWithCursor is
// created and decreased when the same object is destructed. Only used for
// debugging purposes to ensure that we never end up in state where
// `g_ref_count` is larger than one since that could indicate a flickering
// cursor (cursor-less version of the frame is not restored properly and it can
// can lead to visible trails of old cursors).
// See https://crbug.com/1421656#c99 for more details.
int g_ref_count = 0;

uint64_t g_num_flicker_warnings = 0;

// Helper function that blends one image into another. Source image must be
// pre-multiplied with the alpha channel. Destination is assumed to be opaque.
void AlphaBlend(uint8_t* dest,
                int dest_stride,
                const uint8_t* src,
                int src_stride,
                const DesktopSize& size) {
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      uint32_t base_alpha = 255 - src[x * DesktopFrame::kBytesPerPixel + 3];
      if (base_alpha == 255) {
        continue;
      } else if (base_alpha == 0) {
        memcpy(dest + x * DesktopFrame::kBytesPerPixel,
               src + x * DesktopFrame::kBytesPerPixel,
               DesktopFrame::kBytesPerPixel);
      } else {
        dest[x * DesktopFrame::kBytesPerPixel] =
            dest[x * DesktopFrame::kBytesPerPixel] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel];
        dest[x * DesktopFrame::kBytesPerPixel + 1] =
            dest[x * DesktopFrame::kBytesPerPixel + 1] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel + 1];
        dest[x * DesktopFrame::kBytesPerPixel + 2] =
            dest[x * DesktopFrame::kBytesPerPixel + 2] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel + 2];
      }
    }
    src += src_stride;
    dest += dest_stride;
  }
}

#if defined(WEBRTC_WIN)

#endif

class TextureAlphaBlender : public TextureComposer {
 public:
  TextureAlphaBlender();
  ~TextureAlphaBlender() override;
  std::unique_ptr<DesktopFrame>
  MayRestoreFrame(std::unique_ptr<DesktopFrame> src,
                  const DesktopVector& cursor_position,
                  bool cursor_changed) override;

  void ComposeOnFrame(DesktopFrame* dest,
                      const uint8_t* src,
                      int src_stride,
                      const DesktopRect& dest_rect) override;
 private:
  bool InitTextures(const DesktopSize& size);
  std::unique_ptr<DesktopFrame>
  CreateFrameOfLastHandle(const DesktopSize& size);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateRestoredTexture();

  DesktopVector last_cursor_position_;
  DesktopSize size_;
  DesktopRect last_rect_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> original_texture_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> composed_texture_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> last_desktop_texture_;
  std::unique_ptr<DesktopFrame> composed_frame_;
  int device_id_ = -1;
  rtc::scoped_refptr<ScopedHandle> last_handle_;
};

TextureAlphaBlender::TextureAlphaBlender() : size_(0, 0) {}

TextureAlphaBlender::~TextureAlphaBlender() {}

bool TextureAlphaBlender::InitTextures(const DesktopSize& size) {
  original_texture_.Reset();
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = static_cast<UINT>(size.width());
  desc.Height = static_cast<UINT>(size.height());
  desc.Format = DXGI_FORMAT_NV12;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.ArraySize = 1;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MipLevels = 1;
  desc.MiscFlags = 0;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
  if (!pool) {
    RTC_LOG(LS_ERROR) << "Pool uninitialized.";
    return false;
  }
  HRESULT hr = pool->device().d3d_device()->CreateTexture2D(
    &desc, nullptr, original_texture_.GetAddressOf());
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to Create original texture.";
    size_.set(0, 0);
    return false;
  }
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  hr = pool->device().d3d_device()->CreateTexture2D(
    &desc, nullptr, composed_texture_.GetAddressOf());
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to Create composed texture.";
    size_.set(0, 0);
    return false;
  }
  composed_frame_.reset(new BasicDesktopFrame(size));
  size_.set(size.width(), size.height());
  return true;
}

std::unique_ptr<DesktopFrame>
TextureAlphaBlender::CreateFrameOfLastHandle(const DesktopSize& size) {
  if (!last_handle_) {
    return nullptr;
  }
  TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
  if (!pool) {
    RTC_LOG(LS_ERROR) << "Pool uninitialized.";
    return nullptr;
  }
  auto new_handle = pool->GetHandle(last_handle_->id());
  std::unique_ptr<DesktopFrame> texture_frame(
      new BasicDesktopFrame(size));
  texture_frame->set_is_texture(true);
  texture_frame->set_scoped_handle(new_handle);
  return texture_frame;
}

std::unique_ptr<DesktopFrame>
TextureAlphaBlender::MayRestoreFrame(std::unique_ptr<DesktopFrame> src,
                                     const DesktopVector& cursor_position,
                                     bool cursor_changed) {
  RTC_LOG(LS_INFO) << "RestoreFrame.";
  if (!src->may_contain_cursor()) {
    // Clear last compose state.
    last_rect_.set_width(0);
    last_rect_.set_height(0);
    if (last_handle_) {
      last_handle_ = nullptr;
    }
    return src;
  }
  if (last_rect_.is_empty()) {
    // Last frame has no cursor.
    std::unique_ptr<DesktopFrame> last_frame =
        CreateFrameOfLastHandle(src->size());
    if (!last_frame) {
      src->set_may_contain_cursor(false);
      return src;
    } else {
      return last_frame;
    }
  }
  if (last_cursor_position_.equals(cursor_position)) {
    if (!cursor_changed) {
      // Cursor image and position unchanged.
      std::unique_ptr<DesktopFrame> last_frame =
          CreateFrameOfLastHandle(src->size());
      if (!last_frame) {
        return src;
      } else {
        last_frame->set_may_contain_cursor(true);
        return last_frame;
      }
    }
  } else {
    last_cursor_position_.set(cursor_position.x(), cursor_position.y());
  }
  Microsoft::WRL::ComPtr<ID3D11Texture2D> restored_desktop_texture =
      CreateRestoredTexture();
  std::unique_ptr<DesktopFrame> restored_last_frame =
      CreateFrameOfLastHandle(src->size());
  if (!restored_last_frame) {
    return src;
  }
  last_desktop_texture_ = restored_desktop_texture;
  last_rect_.set_width(0);
  last_rect_.set_height(0);
  return restored_last_frame;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
TextureAlphaBlender::CreateRestoredTexture() {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> restored_texture;
  D3D11_TEXTURE2D_DESC desc;
  last_desktop_texture_->GetDesc(&desc);
  TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
  if (!pool) {
    RTC_LOG(LS_ERROR) << "Pool uninitialized.";
    return nullptr;
  }
  auto scoped_handle = pool->GetHandle(DesktopSize(desc.Width, desc.Height));
  last_handle_ = scoped_handle;
  restored_texture = pool->GetTextureOfHandle(scoped_handle->id());
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex_restore;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex_compose;
  HRESULT hr = last_desktop_texture_.As(&mutex_compose);
  RTC_CHECK(SUCCEEDED(hr));
  hr = restored_texture.As(&mutex_restore);
  RTC_CHECK(SUCCEEDED(hr));
  mutex_restore->AcquireSync(0, INFINITE);
  // Copy composed content.
  mutex_compose->AcquireSync(0, INFINITE);
  pool->device().context()->CopyResource(
    static_cast<ID3D11Resource*>(restored_texture.Get()),
    static_cast<ID3D11Resource*>(last_desktop_texture_.Get()));
  mutex_compose->ReleaseSync(0);
  // Restored original rect.
  D3D11_BOX source_region;
  source_region.left = 0;
  source_region.right = last_rect_.width();
  source_region.top = 0;
  source_region.bottom = last_rect_.height();
  source_region.front = 0;
  source_region.back = 1;
  pool->device().context()->CopySubresourceRegion(
      static_cast<ID3D11Resource*>(restored_texture.Get()), 0,
      last_rect_.left(), last_rect_.top(), 0,
      static_cast<ID3D11Resource*>(original_texture_.Get()), 0,
      &source_region);
  mutex_restore->ReleaseSync(0);
  return restored_texture;
}

void TextureAlphaBlender::ComposeOnFrame(DesktopFrame* dest,
                                         const uint8_t* src,
                                         int src_stride,
                                         const DesktopRect& dest_rect) {
  if (!dest->is_texture()) {
    // Only support CopyPixels on texture frame.
    return;
  }
  if (device_id_ < 0) {
    device_id_ = dest->scoped_handle()->device_id();
  } else if (device_id_ != dest->scoped_handle()->device_id()) {
    RTC_LOG(LS_ERROR) << "Conflict device ID for composer.";
    return;
  }

  DesktopRect rect = DesktopRect::MakeXYWH(dest_rect.left(),
                                           dest_rect.top(),
                                           dest_rect.width(),
                                           dest_rect.height());
  if (rect.left() & 1 || rect.top() & 1) {
    // Translate since coordinates should be even.
    rect.Translate((rect.left() & 1), (rect.top() & 1));
  }
  if (rect.width() & 1 || rect.height() & 1) {
    // Crop since texture has even size.
    rect.set_width(rect.width() & ~1);
    rect.set_height(rect.height() & ~1);
  }
  if (rect.width() == 0 || rect.height() == 0) {
    return;
  }
  if (rect.size().width() != size_.width() ||
      rect.size().height() != size_.height()) {
    if (!InitTextures(rect.size())) {
      RTC_LOG(LS_ERROR) << "Failed to Create Staging Texture2D:"
        << rect.size().width() << "," << rect.size().height();
      return;
    }
  }
  // Open shared handle.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> desktop_texture;
  TextureHandlePool* pool = TextureHandlePool::GetInstance(device_id_);
  if (!pool) {
    RTC_LOG(LS_ERROR) << "Pool uninitialized.";
    return;
  }
  desktop_texture = pool->GetTextureOfHandle(dest->scoped_handle()->id());
  HRESULT hr;

  // Copy subRegion of desktop texture to original y and uv textures.
  D3D11_BOX source_region;
  source_region.left = rect.left();
  source_region.right = rect.right();
  source_region.top = rect.top();
  source_region.bottom = rect.bottom();
  source_region.front = 0;
  source_region.back = 1;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  hr = desktop_texture.As(&keyed_mutex);
  RTC_CHECK(SUCCEEDED(hr));
  keyed_mutex->AcquireSync(0, INFINITE);
  pool->device().context()->CopySubresourceRegion(
    static_cast<ID3D11Resource*>(original_texture_.Get()), 0, 0, 0, 0,
    static_cast<ID3D11Resource*>(desktop_texture.Get()), 0,
    &source_region);
  keyed_mutex->ReleaseSync(0);

  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  pool->device().context()->Map(original_texture_.Get(),
      0, D3D11_MAP_READ, 0, &mapped_resource);
  uint8_t* y_data = static_cast<uint8_t*>(mapped_resource.pData);
  int y_stride = mapped_resource.RowPitch;
  // Convert to ARGB for blend
  int ret = libyuv::NV12ToARGB(
      y_data, y_stride,
      y_data + y_stride * rect.height() , y_stride,
      composed_frame_->data(), rect.width() * DesktopFrame::kBytesPerPixel,
      rect.width(), rect.height());
  if (ret != 0) {
    RTC_LOG(LS_ERROR) << "Failed to convert.";
  }
  pool->device().context()->Unmap(original_texture_.Get(), 0);
  AlphaBlend(
      composed_frame_->data(), rect.width() * DesktopFrame::kBytesPerPixel,
      src, src_stride, rect.size());

  // Map composed texture.
  pool->device().context()->Map(composed_texture_.Get(),
      0, D3D11_MAP_WRITE, 0, &mapped_resource);
  y_data = static_cast<uint8_t*>(mapped_resource.pData);
  y_stride = mapped_resource.RowPitch;
  libyuv::ARGBToNV12(
      composed_frame_->data(), rect.width() * DesktopFrame::kBytesPerPixel,
      y_data, y_stride,
      y_data + y_stride * rect.height(), y_stride,
      rect.width(), rect.height());
  // Unmap the composed texture.
  pool->device().context()->Unmap(composed_texture_.Get(), 0);

  D3D11_TEXTURE2D_DESC desc2d;
  composed_texture_->GetDesc(&desc2d);
  source_region.left = 0;
  source_region.right = rect.width();
  source_region.top = 0;
  source_region.bottom = rect.height();
  keyed_mutex->AcquireSync(0, INFINITE);
  pool->device().context()->CopySubresourceRegion(
      static_cast<ID3D11Resource*>(desktop_texture.Get()), 0,
      rect.left(), rect.top(), 0,
      static_cast<ID3D11Resource*>(composed_texture_.Get()), 0,
      &source_region);
  RTC_LOG(LS_INFO) << "ComposeOnFrame succeed.";
  keyed_mutex->ReleaseSync(0);

  last_rect_ = rect;
  last_desktop_texture_ = desktop_texture;
}

// DesktopFrame wrapper that draws mouse on a frame and restores original
// content before releasing the underlying frame.
class DesktopFrameWithCursor : public DesktopFrame {
 public:
  // Takes ownership of `frame`.
  DesktopFrameWithCursor(std::unique_ptr<DesktopFrame> frame,
                         const MouseCursor& cursor,
                         const DesktopVector& position,
                         const DesktopRect& previous_cursor_rect,
                         bool cursor_changed,
                         TextureComposer* composer);
  ~DesktopFrameWithCursor() override;

  DesktopFrameWithCursor(const DesktopFrameWithCursor&) = delete;
  DesktopFrameWithCursor& operator=(const DesktopFrameWithCursor&) = delete;

  DesktopRect cursor_rect() const { return cursor_rect_; }

 private:
  const std::unique_ptr<DesktopFrame> original_frame_;

  DesktopVector restore_position_;
  std::unique_ptr<DesktopFrame> restore_frame_;
  DesktopRect cursor_rect_;
};

DesktopFrameWithCursor::DesktopFrameWithCursor(
    std::unique_ptr<DesktopFrame> frame,
    const MouseCursor& cursor,
    const DesktopVector& position,
    const DesktopRect& previous_cursor_rect,
    bool cursor_changed,
    TextureComposer* composer)
    : DesktopFrame(frame->size(),
                   frame->stride(),
                   frame->data(),
                   frame->shared_memory()),
      original_frame_(std::move(frame)) {
  ++g_ref_count;
  MoveFrameInfoFrom(original_frame_.get());

  DesktopVector image_pos = position.subtract(cursor.hotspot());
  cursor_rect_ = DesktopRect::MakeSize(cursor.image()->size());
  cursor_rect_.Translate(image_pos);
  DesktopVector cursor_origin = cursor_rect_.top_left();
  cursor_rect_.IntersectWith(DesktopRect::MakeSize(size()));

  if (!previous_cursor_rect.equals(cursor_rect_)) {
    mutable_updated_region()->AddRect(cursor_rect_);
    // TODO(crbug:1323241) Update this code to properly handle the case where
    // |previous_cursor_rect| is outside of the boundaries of |frame|.
    // Any boundary check has to take into account the fact that
    // |previous_cursor_rect| can be in DPI or in pixels, based on the platform
    // we're running on.
    mutable_updated_region()->AddRect(previous_cursor_rect);
  } else if (cursor_changed) {
    mutable_updated_region()->AddRect(cursor_rect_);
  }

  if (is_texture()) {
    if (composer && webrtc::field_trial::IsEnabled("CursorOnTexture")) {
      // Blit the cursor on texture.
      DesktopVector origin_shift =
          cursor_rect_.top_left().subtract(cursor_origin);
      composer->ComposeOnFrame(this,
                            cursor.image()->data() +
                            origin_shift.y() * cursor.image()->stride() +
                            origin_shift.x() * DesktopFrame::kBytesPerPixel,
                            cursor.image()->stride(),
                            cursor_rect_);
    }
    return;
  }

  if (cursor_rect_.is_empty())
    return;

  // Copy original screen content under cursor to `restore_frame_`.
  restore_position_ = cursor_rect_.top_left();
  restore_frame_.reset(new BasicDesktopFrame(cursor_rect_.size()));
  restore_frame_->CopyPixelsFrom(*this, cursor_rect_.top_left(),
                                 DesktopRect::MakeSize(restore_frame_->size()));

  // Blit the cursor.
  uint8_t* cursor_rect_data =
      reinterpret_cast<uint8_t*>(data()) + cursor_rect_.top() * stride() +
      cursor_rect_.left() * DesktopFrame::kBytesPerPixel;
  DesktopVector origin_shift = cursor_rect_.top_left().subtract(cursor_origin);
  AlphaBlend(cursor_rect_data, stride(),
             cursor.image()->data() +
                 origin_shift.y() * cursor.image()->stride() +
                 origin_shift.x() * DesktopFrame::kBytesPerPixel,
             cursor.image()->stride(), cursor_rect_.size());
}

DesktopFrameWithCursor::~DesktopFrameWithCursor() {
  if (--g_ref_count > 0) {
    ++g_num_flicker_warnings;
    RTC_LOG(LS_WARNING) << "Cursor might be flickering; number of warnings="
                        << g_num_flicker_warnings;
  }
  // Restore original content of the frame.
  if (restore_frame_) {
    DesktopRect target_rect = DesktopRect::MakeSize(restore_frame_->size());
    target_rect.Translate(restore_position_);
    CopyPixelsFrom(restore_frame_->data(), restore_frame_->stride(),
                   target_rect);
  }
}

}  // namespace

DesktopAndCursorComposer::DesktopAndCursorComposer(
    std::unique_ptr<DesktopCapturer> desktop_capturer,
    const DesktopCaptureOptions& options)
    : DesktopAndCursorComposer(desktop_capturer.release(),
                               MouseCursorMonitor::Create(options).release()) {
  texture_composer_.reset(new TextureAlphaBlender());
}

DesktopAndCursorComposer::DesktopAndCursorComposer(
    DesktopCapturer* desktop_capturer,
    MouseCursorMonitor* mouse_monitor)
    : desktop_capturer_(desktop_capturer), mouse_monitor_(mouse_monitor) {
  RTC_DCHECK(desktop_capturer_);
}

DesktopAndCursorComposer::~DesktopAndCursorComposer() = default;

std::unique_ptr<DesktopAndCursorComposer>
DesktopAndCursorComposer::CreateWithoutMouseCursorMonitor(
    std::unique_ptr<DesktopCapturer> desktop_capturer) {
  return std::unique_ptr<DesktopAndCursorComposer>(
      new DesktopAndCursorComposer(desktop_capturer.release(), nullptr));
}

void DesktopAndCursorComposer::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;
  if (mouse_monitor_)
    mouse_monitor_->Init(this, MouseCursorMonitor::SHAPE_AND_POSITION);
  desktop_capturer_->Start(this);
}

void DesktopAndCursorComposer::SetMaxFrameRate(uint32_t max_frame_rate) {
  desktop_capturer_->SetMaxFrameRate(max_frame_rate);
}

void DesktopAndCursorComposer::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  desktop_capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
}

void DesktopAndCursorComposer::CaptureFrame() {
  if (mouse_monitor_)
    mouse_monitor_->Capture();
  desktop_capturer_->CaptureFrame();
}

void DesktopAndCursorComposer::SetExcludedWindow(WindowId window) {
  desktop_capturer_->SetExcludedWindow(window);
}

bool DesktopAndCursorComposer::GetSourceList(SourceList* sources) {
  return desktop_capturer_->GetSourceList(sources);
}

bool DesktopAndCursorComposer::SelectSource(SourceId id) {
  return desktop_capturer_->SelectSource(id);
}

bool DesktopAndCursorComposer::FocusOnSelectedSource() {
  return desktop_capturer_->FocusOnSelectedSource();
}

bool DesktopAndCursorComposer::IsOccluded(const DesktopVector& pos) {
  return desktop_capturer_->IsOccluded(pos);
}

#if defined(WEBRTC_USE_GIO)
DesktopCaptureMetadata DesktopAndCursorComposer::GetMetadata() {
  return desktop_capturer_->GetMetadata();
}
#endif  // defined(WEBRTC_USE_GIO)

void DesktopAndCursorComposer::OnFrameCaptureStart() {
  callback_->OnFrameCaptureStart();
}

void DesktopAndCursorComposer::OnCaptureResult(
    DesktopCapturer::Result result,
    std::unique_ptr<DesktopFrame> frame) {
  if (frame) {
    if (frame->is_texture() &&
        webrtc::field_trial::IsEnabled("CursorOnTexture")) {
      // if (!webrtc::field_trial::IsEnabled("CursorOnTexture")) {
      //   callback_->OnCaptureResult(result, std::move(frame));
      //   return;
      // }
      // Restore frame without cursor.
      bool update_cursor = cursor_changed_;
      if (desktop_capturer_->IsOccluded(cursor_position_)) {
        update_cursor = true;
      }
      auto restored_frame = texture_composer_->MayRestoreFrame(
          std::move(frame), cursor_position_, update_cursor);
      frame = std::move(restored_frame);
    }
  }

  if (frame && cursor_) {
    if (!frame->may_contain_cursor() &&
        frame->rect().Contains(cursor_position_) &&
        !desktop_capturer_->IsOccluded(cursor_position_)) {
      DesktopVector relative_position =
          cursor_position_.subtract(frame->top_left());
#if defined(WEBRTC_MAC) || defined(CHROMEOS)
      // On OSX, the logical(DIP) and physical coordinates are used mixingly.
      // For example, the captured cursor has its size in physical pixels(2x)
      // and location in logical(DIP) pixels on Retina monitor. This will cause
      // problem when the desktop is mixed with Retina and non-Retina monitors.
      // So we use DIP pixel for all location info and compensate with the scale
      // factor of current frame to the `relative_position`.
      const float scale = frame->scale_factor();
      relative_position.set(relative_position.x() * scale,
                            relative_position.y() * scale);
#endif
      auto frame_with_cursor = std::make_unique<DesktopFrameWithCursor>(
          std::move(frame), *cursor_, relative_position, previous_cursor_rect_,
          cursor_changed_, texture_composer_.get());
      previous_cursor_rect_ = frame_with_cursor->cursor_rect();
      cursor_changed_ = false;
      frame = std::move(frame_with_cursor);
      frame->set_may_contain_cursor(true);
    }
  }

  callback_->OnCaptureResult(result, std::move(frame));
}

void DesktopAndCursorComposer::OnMouseCursor(MouseCursor* cursor) {
  cursor_changed_ = true;
  cursor_.reset(cursor);
}

void DesktopAndCursorComposer::OnMouseCursorPosition(
    const DesktopVector& position) {
  cursor_position_ = position;
}

}  // namespace webrtc
