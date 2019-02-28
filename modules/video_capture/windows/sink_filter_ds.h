/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_

#include <streams.h>  // Include base DS filter header files
#include <memory>

#include "modules/video_capture/video_capture_defines.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace videocapturemodule {
// forward declaration

class CaptureSinkFilter;
/**
 *     input pin for camera input
 *
 */
class CaptureInputPin : public CBaseInputPin {
 public:
  CaptureInputPin(LPCTSTR object_name,
                  CaptureSinkFilter* filter,
                  CCritSec* lock,
                  HRESULT* hr,
                  LPCWSTR name);
  ~CaptureInputPin() override;

  HRESULT GetMediaType(int pos, CMediaType* media_type) override;
  HRESULT CheckMediaType(const CMediaType* media_type) override;
  HRESULT SetMatchingMediaType(const VideoCaptureCapability& capability);

  STDMETHOD(Receive)(IN IMediaSample*) override;

 private:
  VideoCaptureCapability requested_capability_;
  VideoCaptureCapability resulting_capability_;
  HANDLE thread_handle_ = nullptr;
};

class CaptureSinkFilter : public CBaseFilter {
 public:
  CaptureSinkFilter(LPCTSTR object_name,
                    IUnknown* unknown,
                    HRESULT* hr,
                    VideoCaptureExternal* capture_observer);
  ~CaptureSinkFilter() override;

  void ProcessCapturedFrame(unsigned char* buffer,
                            size_t length,
                            const VideoCaptureCapability& frame_info)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(receiver_lock_);

  //  --------------------------------------------------------------------
  //  COM interfaces
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
  STDMETHOD_(ULONG, AddRef)() override;
  STDMETHOD_(ULONG, Release)() override;
  STDMETHOD(SetMatchingMediaType)(const VideoCaptureCapability& capability);

  //  --------------------------------------------------------------------
  //  CBaseFilter methods
  int GetPinCount() override;
  CBasePin* GetPin(IN int Index) override;

  STDMETHOD(Pause)() override;
  STDMETHOD(Stop)() override;
  STDMETHOD(GetClassID)(CLSID* clsid) override;

  // TODO(tommi): Is this lock needed?
  const rtc::CriticalSection receiver_lock_;

 private:
  CCritSec filter_lock_;
  const std::unique_ptr<CaptureInputPin> input_pin_;
  VideoCaptureExternal* capture_observer_;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif  // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_
