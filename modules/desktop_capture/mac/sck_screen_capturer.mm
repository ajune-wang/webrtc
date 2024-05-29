/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/sck_screen_capturer.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

using webrtc::DesktopFrameIOSurface;

namespace webrtc {
class SckScreenCapturer;
}  // namespace webrtc

API_AVAILABLE(macos(12.3))
@interface SckHelper : NSObject <SCStreamDelegate, SCStreamOutput>

- (instancetype)initWithCapturer:(webrtc::SckScreenCapturer*)capturer;

@end

namespace webrtc {

class API_AVAILABLE(macos(12.3)) SckScreenCapturer final : public DesktopCapturer {
 public:
  explicit SckScreenCapturer(const DesktopCaptureOptions& options);

  SckScreenCapturer(const SckScreenCapturer&) = delete;
  SckScreenCapturer& operator=(const SckScreenCapturer&) = delete;

  ~SckScreenCapturer() override;

  // DesktopCapturer interface.
  void Start(DesktopCapturer::Callback* callback) override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
  void CaptureFrame() override;
  bool SelectSource(SourceId id) override;

  // Called by SckHelper to notify of a newly captured frame.
  void OnNewIOSurface(IOSurfaceRef io_surface);

 private:
  void OnShareableContentCreated(SCShareableContent* content);

  SckHelper* helper_;

  // Provides captured desktop frames.
  SCStream* stream_;

  Callback* callback_ = nullptr;

  // Currently selected display, or 0 if the full desktop is selected. This capturer does not
  // support full-desktop capture, and will fall back to the first display.
  CGDirectDisplayID current_display_ = 0;

  Mutex latest_frame_lock_;
  std::unique_ptr<SharedDesktopFrame> latest_frame_ RTC_GUARDED_BY(latest_frame_lock_);
};

SckScreenCapturer::SckScreenCapturer(const DesktopCaptureOptions& options) {
  helper_ = [[SckHelper alloc] initWithCapturer:this];
}

SckScreenCapturer::~SckScreenCapturer() {
  // XXX - stop the stream, so that capture samples are not sent to a deleted object.
}

void SckScreenCapturer::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;

  auto handler = ^(SCShareableContent* content, NSError* error) {
    // XXX - cancel this callback on destruction.
    OnShareableContentCreated(content);
  };

  [SCShareableContent getShareableContentWithCompletionHandler:handler];
}

void SckScreenCapturer::SetMaxFrameRate(uint32_t max_frame_rate) {}

void SckScreenCapturer::CaptureFrame() {
  std::unique_ptr<DesktopFrame> frame;
  {
    MutexLock lock(&latest_frame_lock_);
    if (latest_frame_) {
      frame = latest_frame_->Share();
    }
  }
  if (frame) {
    // TODO: lambroslambrou - track damage regions.
    frame->mutable_updated_region()->AddRect(DesktopRect::MakeSize(frame->size()));
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
  } else {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
  }
}

bool SckScreenCapturer::SelectSource(SourceId id) {
  if (id == kFullDesktopScreenId) {
    current_display_ = 0;
  } else {
    current_display_ = id;
  }
  return true;
}

void SckScreenCapturer::OnShareableContentCreated(SCShareableContent* content) {
  if (!content) {
    RTC_LOG(LS_ERROR) << "getShareableContent failed.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (!content.displays.count) {
    RTC_LOG(LS_ERROR) << "getShareableContent returned no displays.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  SCDisplay* captured_display;
  for (SCDisplay* display in content.displays) {
    if (current_display_ == display.displayID) {
      captured_display = display;
      break;
    }
  }
  if (!captured_display) {
    RTC_LOG(LS_WARNING) << "Display " << current_display_
                        << " not found, falling back to first display.";
    captured_display = content.displays.firstObject;
  }

  SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:captured_display
                                                    excludingWindows:@[]];
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  config.pixelFormat = kCVPixelFormatType_32BGRA;
  config.showsCursor = NO;
  config.width = filter.contentRect.size.width * filter.pointPixelScale;
  config.height = filter.contentRect.size.height * filter.pointPixelScale;

  if (@available(macOS 14.0, *)) {
    config.captureResolution = SCCaptureResolutionNominal;
  }

  stream_ = [[SCStream alloc] initWithFilter:filter configuration:config delegate:helper_];

  NSError* add_stream_output_error = nil;
  bool add_stream_output_result = [stream_ addStreamOutput:helper_
                                                      type:SCStreamOutputTypeScreen
                                        sampleHandlerQueue:dispatch_get_main_queue()
                                                     error:&add_stream_output_error];
  if (!add_stream_output_result) {
    stream_ = nil;
    RTC_LOG(LS_ERROR) << "addStreamOutput failed.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  auto handler = ^(NSError* error) {
    if (error) {
      RTC_LOG(LS_ERROR) << "startCaptureWithCompletionHandler failed.";
    } else {
      RTC_LOG(LS_INFO) << "Capture started.";
    }
  };

  [stream_ startCaptureWithCompletionHandler:handler];
}

void SckScreenCapturer::OnNewIOSurface(IOSurfaceRef io_surface) {
  rtc::ScopedCFTypeRef<IOSurfaceRef> scoped_io_surface(io_surface, rtc::RetainPolicy::RETAIN);
  std::unique_ptr<DesktopFrameIOSurface> desktop_frame_io_surface =
      DesktopFrameIOSurface::Wrap(scoped_io_surface);
  if (!desktop_frame_io_surface) {
    RTC_LOG(LS_ERROR) << "Failed to lock IOSurface.";
    return;
  }

  std::unique_ptr<SharedDesktopFrame> frame =
      SharedDesktopFrame::Wrap(std::move(desktop_frame_io_surface));
  {
    MutexLock lock(&latest_frame_lock_);
    std::swap(latest_frame_, frame);
  }
}

std::unique_ptr<DesktopCapturer> CreateSckScreenCapturer(const DesktopCaptureOptions& options) {
  if (@available(macOS 12.3, *)) {
    return std::make_unique<SckScreenCapturer>(options);
  } else {
    return nullptr;
  }
}

}  // namespace webrtc

@implementation SckHelper {
  webrtc::SckScreenCapturer* _capturer;
}

- (instancetype)initWithCapturer:(webrtc::SckScreenCapturer*)capturer {
  if (self = [super init]) {
    _capturer = capturer;
  }
  return self;
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer) {
    return;
  }

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface) {
    return;
  }

  _capturer->OnNewIOSurface(ioSurface);
}

@end
