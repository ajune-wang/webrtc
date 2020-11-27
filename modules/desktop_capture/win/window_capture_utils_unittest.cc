/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capture_utils.h"

#include <tchar.h>
#include <winuser.h>
#include <algorithm>
#include <memory>
#include <mutex>

#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const TCHAR kWindowClass[] = _T("WindowCaptureUtilsTestClass");
const TCHAR kWindowTitle[] = _T("Window Capture Utils Test");
const int kWindowWidth = 300;
const int kWindowHeight = 200;

struct WindowInfo {
  HWND hwnd;
  HINSTANCE window_instance;
  ATOM window_class;
};

WindowInfo CreateTestWindow(const TCHAR* window_title) {
  WindowInfo info;
  ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCTSTR>(&::DefWindowProc),
                      &info.window_instance);

  WNDCLASSEX wcex;
  memset(&wcex, 0, sizeof(wcex));
  wcex.cbSize = sizeof(wcex);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.hInstance = info.window_instance;
  wcex.lpfnWndProc = &::DefWindowProc;
  wcex.lpszClassName = kWindowClass;
  info.window_class = ::RegisterClassEx(&wcex);

  info.hwnd = ::CreateWindow(kWindowClass, window_title, WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth,
                             kWindowHeight, /*parent_window=*/nullptr,
                             /*menu_bar=*/nullptr, info.window_instance,
                             /*additional_params=*/nullptr);

  ::ShowWindow(info.hwnd, SW_SHOWNORMAL);
  ::UpdateWindow(info.hwnd);
  return info;
}

void DestroyTestWindow(WindowInfo info) {
  ::DestroyWindow(info.hwnd);
  ::UnregisterClass(MAKEINTATOM(info.window_class), info.window_instance);
}

}  // namespace

TEST(WindowCaptureUtilsTest, GetWindowList) {
  WindowInfo info = CreateTestWindow(kWindowTitle);
  DesktopCapturer::SourceList window_list;
  ASSERT_TRUE(GetWindowList(GetWindowListFlags::kNone, &window_list));
  EXPECT_GT(window_list.size(), 0ULL);
  EXPECT_NE(std::find_if(window_list.begin(), window_list.end(),
                         [&info](DesktopCapturer::Source window) {
                           return reinterpret_cast<HWND>(window.id) ==
                                  info.hwnd;
                         }),
            window_list.end());
  DestroyTestWindow(info);
}

TEST(WindowCaptureUtilsTest, IgnoreUnresponsiveWindows) {
  std::unique_ptr<rtc::Thread> window_thread;
  window_thread = rtc::Thread::Create();
  window_thread->SetName("wgc_test_window_thread", nullptr);
  window_thread->Start();

  WindowInfo info;
  window_thread->Invoke<void>(
      RTC_FROM_HERE, [&info]() { info = CreateTestWindow(kWindowTitle); });

  // Intentionally create a deadlock to cause the window to become unresponsive.
  std::mutex mtx;
  mtx.lock();
  window_thread->PostTask(RTC_FROM_HERE, [&mtx]() {
    mtx.lock();
    mtx.unlock();
  });

  EXPECT_FALSE(IsWindowResponding(info.hwnd));

  // Check that the window is included if we do not specify kIgnoreUnresponsive.
  DesktopCapturer::SourceList window_list;
  ASSERT_TRUE(GetWindowList(GetWindowListFlags::kNone, &window_list));
  EXPECT_GT(window_list.size(), 0ULL);
  EXPECT_NE(std::find_if(window_list.begin(), window_list.end(),
                         [&info](DesktopCapturer::Source window) {
                           return reinterpret_cast<HWND>(window.id) ==
                                  info.hwnd;
                         }),
            window_list.end());

  window_list.clear();

  // Check that the window is ignored when we specfy kIgnoreUnresponsive.
  ASSERT_TRUE(
      GetWindowList(GetWindowListFlags::kIgnoreUnresponsive, &window_list));
  EXPECT_EQ(std::find_if(window_list.begin(), window_list.end(),
                         [&info](DesktopCapturer::Source window) {
                           return reinterpret_cast<HWND>(window.id) ==
                                  info.hwnd;
                         }),
            window_list.end());

  mtx.unlock();
  window_thread->Invoke<void>(RTC_FROM_HERE,
                              [&info]() { DestroyTestWindow(info); });
  window_thread->Stop();
}

TEST(WindowCaptureUtilsTest, IgnoreUntitledWindows) {
  WindowInfo info = CreateTestWindow(_T(""));

  // Check that the window is included if we do not specify kIgnoreUntitled.
  DesktopCapturer::SourceList window_list;
  ASSERT_TRUE(GetWindowList(GetWindowListFlags::kNone, &window_list));
  EXPECT_GT(window_list.size(), 0ULL);
  EXPECT_NE(std::find_if(window_list.begin(), window_list.end(),
                         [&info](DesktopCapturer::Source window) {
                           return reinterpret_cast<HWND>(window.id) ==
                                  info.hwnd;
                         }),
            window_list.end());

  window_list.clear();

  // Check that the window is ignored when we specfy kIgnoreUntitled.
  ASSERT_TRUE(GetWindowList(GetWindowListFlags::kIgnoreUntitled, &window_list));
  EXPECT_EQ(std::find_if(window_list.begin(), window_list.end(),
                         [&info](DesktopCapturer::Source window) {
                           return reinterpret_cast<HWND>(window.id) ==
                                  info.hwnd;
                         }),
            window_list.end());

  DestroyTestWindow(info);
}

}  // namespace webrtc
