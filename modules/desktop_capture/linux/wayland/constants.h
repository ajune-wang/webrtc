/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_CONSTANTS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_CONSTANTS_H_
#include <stdint.h>

namespace webrtc {
const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

// Defines what type of stream should be captured.
// Values are set based on source type property in
// xdg-desktop-portal/screencast
// https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
enum class CaptureSourceType : uint32_t {
  kScreen = 0b01,
  kWindow = 0b10,
  kAnyScreenContent = kScreen | kWindow
};

// Defines whether or not cursor should be captured as part of screencast.
// Values are set based on cursor mode property in
// xdg-desktop-portal/screencast
// https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
enum class CursorMode : uint32_t {
  // Mouse cursor will not be included in any form
  kHidden = 0b01,
  // Mouse cursor will be part of the screen content
  kEmbedded = 0b10,
  // Mouse cursor information will be send separately in form of metadata
  kMetadata = 0b100
};

enum class RequestResponse {
  // Success, the request is carried out.
  kSuccess,
  // The user cancelled the interaction.
  kUserCancelled,
  // The user interaction was ended in some other way.
  kError,

  kMaxValue = kError
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_CONSTANTS_H_
