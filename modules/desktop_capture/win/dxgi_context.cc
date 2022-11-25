/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/dxgi_context.h"

#include "modules/desktop_capture/win/dxgi_duplicator_controller.h"
#include "rtc_base/logging.h"

namespace webrtc {

DxgiAdapterContext::DxgiAdapterContext() = default;
DxgiAdapterContext::DxgiAdapterContext(const DxgiAdapterContext& context) =
    default;
DxgiAdapterContext::~DxgiAdapterContext() = default;

DxgiFrameContext::DxgiFrameContext() = default;

DxgiFrameContext::~DxgiFrameContext() {
  RTC_LOG(LS_INFO) << __func__;

  Reset();
}

void DxgiFrameContext::Reset() {
  RTC_LOG(LS_INFO) << __func__;

  DxgiDuplicatorController::Instance()->Unregister(this);
  controller_id = 0;
}

}  // namespace webrtc
