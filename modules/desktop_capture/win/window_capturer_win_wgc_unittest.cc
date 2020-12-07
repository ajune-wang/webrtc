/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capturer_win_wgc.h"

#include <string>
#include <utility>
#include <vector>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/test_support/test_window.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/win/scoped_com_initializer.h"
#include "rtc_base/win/windows_version.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const char kWindowThreadName[] = "wgc_capturer_test_window_thread";
const WCHAR kWindowTitle[] = L"WGC Capturer Test Window";

const int kSmallWindowWidth = 200;
const int kSmallWindowHeight = 100;
const int kWindowWidth = 300;
const int kWindowHeight = 200;
const int kLargeWindowWidth = 400;
const int kLargeWindowHeight = 300;

// The size of the image we capture is slightly smaller than the actual size of
// the window.
const int kWindowWidthSubtrahend = 14;
const int kWindowHeightSubtrahend = 7;

// Custom message constants so we can direct our threads to close windows
// and quit running.
const UINT kNoOp = WM_APP;
const UINT kDestroyWindow = WM_APP + 1;
const UINT kQuitRunning = WM_APP + 2;

}  // namespace

class WindowCapturerWinWgcTest : public ::testing::Test,
                                 public DesktopCapturer::Callback {
 public:
  void SetUp() override {
    if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
      RTC_LOG(LS_INFO)
          << "Skipping WindowCapturerWinWgcTests on Windows versions < RS5.";
      GTEST_SKIP();
    }

    com_initializer_ =
        std::make_unique<ScopedCOMInitializer>(ScopedCOMInitializer::kMTA);
    EXPECT_TRUE(com_initializer_->Succeeded());

    capturer_ = WindowCapturerWinWgc::CreateRawWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
  }

  void TearDown() override {
    if (window_open_) {
      CloseTestWindow();
    }
  }

  void OpenTestWindow() {
    window_info_ = CreateTestWindow(kWindowTitle, kWindowHeight, kWindowWidth);
    window_open_ = true;

    while (!IsWindowResponding(window_info_.hwnd)) {
      RTC_LOG(LS_INFO) << "Waiting for test window to become responsive in "
                          "WindowCapturerWinWgcTest.";
    }

    while (!IsWindowValidAndVisible(window_info_.hwnd)) {
      RTC_LOG(LS_INFO) << "Waiting for test window to be visible in "
                          "WindowCapturerWinWgcTest.";
    }
  }

  void CloseTestWindow() {
    DestroyTestWindow(window_info_);
    window_open_ = false;
  }

  // The window must live on a separate thread from where the capturer is
  // created if we are interested in having the GraphicsCaptureItem events fire.
  void CreateWindowOnSeparateThread() {
    window_thread_ = rtc::Thread::Create();
    window_thread_->SetName(kWindowThreadName, nullptr);
    window_thread_->Start();
    window_thread_->Invoke<void>(RTC_FROM_HERE, [this]() {
      window_thread_id_ = GetCurrentThreadId();
      OpenTestWindow();
    });

    ASSERT_TRUE(window_thread_->RunningForTest());
    ASSERT_FALSE(window_thread_->IsCurrent());
  }

  void StartWindowThreadMessageLoop() {
    window_thread_->PostTask(RTC_FROM_HERE, [this]() {
      MSG msg;
      BOOL gm;
      while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
        ::DispatchMessage(&msg);
        if (msg.message == kDestroyWindow) {
          CloseTestWindow();
        }
        if (msg.message == kQuitRunning) {
          PostQuitMessage(0);
        }
        if (msg.message == WM_QUIT) {
          break;
        }
      }
    });
  }

  DesktopCapturer::SourceId FindWindowId() {
    // Frequently, the test window will not show up GetSourceList because it
    // was created too recently. Since we are confident the window will be found
    // eventually we loop here until we find it.
    intptr_t src_id;
    do {
      DesktopCapturer::SourceList sources;
      EXPECT_TRUE(capturer_->GetSourceList(&sources));

      auto it = std::find_if(
          sources.begin(), sources.end(),
          [&](const DesktopCapturer::Source& src) {
            return src.id == reinterpret_cast<intptr_t>(window_info_.hwnd);
          });

      src_id = it->id;
    } while (src_id != reinterpret_cast<intptr_t>(window_info_.hwnd));

    return src_id;
  }

  void DoCapture() {
    // Sometimes the first few frames are empty becaues the capture engine is
    // still starting up. We also may drop a few frames when the window is
    // resized or un-minimized.
    do {
      capturer_->CaptureFrame();
    } while (result_ == DesktopCapturer::Result::ERROR_TEMPORARY);

    EXPECT_EQ(result_, DesktopCapturer::Result::SUCCESS);
    EXPECT_TRUE(frame_);
  }

  // DesktopCapturer::Callback interface
  // The capturer synchronously invokes this method before |CaptureFrame()|
  // returns.
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    result_ = result;
    frame_ = std::move(frame);
  }

 protected:
  std::unique_ptr<ScopedCOMInitializer> com_initializer_;
  DWORD capturer_thread_id_;
  DWORD window_thread_id_;
  std::unique_ptr<rtc::Thread> capturer_thread_;
  std::unique_ptr<rtc::Thread> window_thread_;
  WindowInfo window_info_;
  bool window_open_ = false;
  DesktopCapturer::Result result_;
  std::unique_ptr<DesktopFrame> frame_;
  std::unique_ptr<DesktopCapturer> capturer_;
};

TEST_F(WindowCapturerWinWgcTest, SelectValidSource) {
  OpenTestWindow();
  DesktopCapturer::SourceId src_id = FindWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));
}

TEST_F(WindowCapturerWinWgcTest, SelectInvalidSource) {
  DesktopCapturer::Source invalid_src;
  EXPECT_FALSE(capturer_->SelectSource(invalid_src.id));

  invalid_src.id = 0x0000;
  EXPECT_FALSE(capturer_->SelectSource(invalid_src.id));
}

TEST_F(WindowCapturerWinWgcTest, SelectMinimizedSource) {
  OpenTestWindow();
  DesktopCapturer::SourceId src_id = FindWindowId();
  MinimizeTestWindow(reinterpret_cast<HWND>(src_id));
  EXPECT_FALSE(capturer_->SelectSource(src_id));

  UnminimizeTestWindow(reinterpret_cast<HWND>(src_id));
  EXPECT_TRUE(capturer_->SelectSource(src_id));
}

TEST_F(WindowCapturerWinWgcTest, SelectClosedSource) {
  OpenTestWindow();
  DesktopCapturer::SourceId src_id = FindWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  CloseTestWindow();
  EXPECT_FALSE(capturer_->SelectSource(src_id));
}

TEST_F(WindowCapturerWinWgcTest, Capture) {
  OpenTestWindow();
  DesktopCapturer::SourceId src_id = FindWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);
}

TEST_F(WindowCapturerWinWgcTest, ResizeWindowMidCapture) {
  OpenTestWindow();
  DesktopCapturer::SourceId src_id = FindWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);

  ResizeTestWindow(window_info_.hwnd, kLargeWindowWidth, kLargeWindowHeight);
  DoCapture();
  // We don't expect to see the new size until the next capture, as the frame
  // pool won't have had a chance to resize yet.
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kLargeWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kLargeWindowHeight - kWindowHeightSubtrahend);

  ResizeTestWindow(window_info_.hwnd, kSmallWindowWidth, kSmallWindowHeight);
  DoCapture();
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kSmallWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(),
            kSmallWindowHeight - kWindowHeightSubtrahend);
}

TEST_F(WindowCapturerWinWgcTest, MinimizeWindowMidCapture) {
  OpenTestWindow();
  DesktopCapturer::SourceId src_id = FindWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);

  // Minmize the window and capture should continue but return temporary errors.
  MinimizeTestWindow(window_info_.hwnd);
  for (int i = 0; i < 10; ++i) {
    capturer_->CaptureFrame();
    EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_TEMPORARY);
  }

  // Reopen the window and the capture should continue normally.
  UnminimizeTestWindow(window_info_.hwnd);

  DoCapture();
  // We can't verify the window size here because the test window does not
  // repaint itself after it is unminimized, but capturing successfully is still
  // a good test.
}

TEST_F(WindowCapturerWinWgcTest, CloseWindowMidCapture) {
  // This test depends on GraphicsCaptureItem events being pumped, so we must
  // create the window on a separate thread and start a message pump.
  CreateWindowOnSeparateThread();
  StartWindowThreadMessageLoop();
  DesktopCapturer::SourceId src_id = FindWindowId();
  EXPECT_TRUE(capturer_->SelectSource(src_id));

  capturer_->Start(this);
  DoCapture();
  EXPECT_EQ(frame_->size().width(), kWindowWidth - kWindowWidthSubtrahend);
  EXPECT_EQ(frame_->size().height(), kWindowHeight - kWindowHeightSubtrahend);

  // Close the window and stop its thread.
  ::PostThreadMessage(window_thread_id_, kDestroyWindow, 0, 0);
  ::PostThreadMessage(window_thread_id_, kQuitRunning, 0, 0);
  window_thread_->Stop();

  // We need to call GetMessage to trigger the Closed event and the capturer's
  // event handler for it. If we are too early and the Closed event hasn't
  // arrived yet we should keep trying until the capturer receives it and stops.
  auto iter = static_cast<WindowCapturerWinWgc*>(capturer_.get())
                  ->ongoing_captures_.find(src_id);
  WgcCaptureSession* capture_session = nullptr;
  capture_session = &iter->second;
  while (capture_session->IsCaptureStarted()) {
    // Since the capturer handles the Closed message, there will be no message
    // for us we and GetMessage will hang, unless we send ourselves a message
    // first.
    ::PostThreadMessage(GetCurrentThreadId(), kNoOp, 0, 0);
    MSG msg;
    ::GetMessage(&msg, NULL, 0, 0);
    ::DispatchMessage(&msg);
  }

  // Occasionally, one last frame will have made it into the frame pool before
  // the window closed. The first call will consume it, and in that case we need
  // to make one more call to CaptureFrame.
  capturer_->CaptureFrame();
  if (result_ == DesktopCapturer::Result::SUCCESS)
    capturer_->CaptureFrame();

  EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_PERMANENT);
}

}  // namespace webrtc
