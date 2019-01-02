/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_FAKE_WEB_RTC_VCM_FACTORY_H_
#define MEDIA_ENGINE_FAKE_WEB_RTC_VCM_FACTORY_H_

#include <vector>

#include "media/engine/fake_web_rtc_device_info.h"
#include "media/engine/fake_web_rtc_video_capture_module.h"
#include "media/engine/web_rtc_video_capturer.h"

// Factory class to allow the fakes above to be injected into
// WebRtcVideoCapturer.
class FakeWebRtcVcmFactory : public cricket::WebRtcVcmFactoryInterface {
 public:
  virtual rtc::scoped_refptr<webrtc::VideoCaptureModule> Create(
      const char* device_id) {
    if (!device_info.GetDeviceById(device_id))
      return NULL;
    rtc::scoped_refptr<FakeWebRtcVideoCaptureModule> module(
        new rtc::RefCountedObject<FakeWebRtcVideoCaptureModule>());
    modules.push_back(module);
    return module;
  }
  virtual webrtc::VideoCaptureModule::DeviceInfo* CreateDeviceInfo() {
    return &device_info;
  }
  virtual void DestroyDeviceInfo(webrtc::VideoCaptureModule::DeviceInfo* info) {
  }
  FakeWebRtcDeviceInfo device_info;
  std::vector<rtc::scoped_refptr<FakeWebRtcVideoCaptureModule>> modules;
};

#endif  // MEDIA_ENGINE_FAKE_WEB_RTC_VCM_FACTORY_H_
