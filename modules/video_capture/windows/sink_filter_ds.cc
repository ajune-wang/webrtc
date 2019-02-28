/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/windows/sink_filter_ds.h"

#include "modules/video_capture/windows/help_functions_ds.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/string_utils.h"

#include <dvdmedia.h>  // VIDEOINFOHEADER2
#include <initguid.h>

DEFINE_GUID(CLSID_SINKFILTER,
            0x88cdbbdc,
            0xa73b,
            0x4afa,
            0xac,
            0xbf,
            0x15,
            0xd5,
            0xe2,
            0xce,
            0x12,
            0xc3);

namespace webrtc {
namespace videocapturemodule {

CaptureInputPin::CaptureInputPin(LPCTSTR object_name,
                                 CaptureSinkFilter* filter,
                                 CCritSec* lock,
                                 HRESULT* hr,
                                 LPCWSTR name)
    : CBaseInputPin(object_name, filter, lock, hr, name) {}

CaptureInputPin::~CaptureInputPin() {}

HRESULT CaptureInputPin::GetMediaType(int pos, CMediaType* media_type) {
  // reset the thread handle
  thread_handle_ = nullptr;

  if (pos < 0)
    return E_INVALIDARG;

  VIDEOINFOHEADER* pvi = reinterpret_cast<VIDEOINFOHEADER*>(
      media_type->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
  if (!pvi) {
    RTC_LOG(LS_INFO) << "CheckMediaType VIDEOINFOHEADER is NULL. Returning.";
    return E_OUTOFMEMORY;
  }

  ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));
  pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pvi->bmiHeader.biPlanes = 1;
  pvi->bmiHeader.biClrImportant = 0;
  pvi->bmiHeader.biClrUsed = 0;
  if (requested_capability_.maxFPS != 0) {
    pvi->AvgTimePerFrame = 10000000 / requested_capability_.maxFPS;
  }

  SetRectEmpty(&(pvi->rcSource));  // we want the whole image area rendered.
  SetRectEmpty(&(pvi->rcTarget));  // no particular destination rectangle

  media_type->SetType(&MEDIATYPE_Video);
  media_type->SetFormatType(&FORMAT_VideoInfo);
  media_type->SetTemporalCompression(FALSE);

  int32_t positionOffset = 1;
  switch (pos + positionOffset) {
    case 0: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
      pvi->bmiHeader.biBitCount = 12;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_capability_.width;
      pvi->bmiHeader.biHeight = requested_capability_.height;
      pvi->bmiHeader.biSizeImage =
          3 * requested_capability_.height * requested_capability_.width / 2;
      media_type->SetSubtype(&MEDIASUBTYPE_I420);
    } break;
    case 1: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
      pvi->bmiHeader.biBitCount = 16;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_capability_.width;
      pvi->bmiHeader.biHeight = requested_capability_.height;
      pvi->bmiHeader.biSizeImage =
          2 * requested_capability_.width * requested_capability_.height;
      media_type->SetSubtype(&MEDIASUBTYPE_YUY2);
    } break;
    case 2: {
      pvi->bmiHeader.biCompression = BI_RGB;
      pvi->bmiHeader.biBitCount = 24;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_capability_.width;
      pvi->bmiHeader.biHeight = requested_capability_.height;
      pvi->bmiHeader.biSizeImage =
          3 * requested_capability_.height * requested_capability_.width;
      media_type->SetSubtype(&MEDIASUBTYPE_RGB24);
    } break;
    case 3: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('U', 'Y', 'V', 'Y');
      pvi->bmiHeader.biBitCount = 16;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_capability_.width;
      pvi->bmiHeader.biHeight = requested_capability_.height;
      pvi->bmiHeader.biSizeImage =
          2 * requested_capability_.height * requested_capability_.width;
      media_type->SetSubtype(&MEDIASUBTYPE_UYVY);
    } break;
    case 4: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('M', 'J', 'P', 'G');
      pvi->bmiHeader.biBitCount = 12;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_capability_.width;
      pvi->bmiHeader.biHeight = requested_capability_.height;
      pvi->bmiHeader.biSizeImage =
          3 * requested_capability_.height * requested_capability_.width / 2;
      media_type->SetSubtype(&MEDIASUBTYPE_MJPG);
    } break;
    default:
      return VFW_S_NO_MORE_ITEMS;
  }

  media_type->SetSampleSize(pvi->bmiHeader.biSizeImage);
  RTC_LOG(LS_INFO) << "GetMediaType position " << pos << ", width "
                   << requested_capability_.width << ", height "
                   << requested_capability_.height << ", biCompression 0x"
                   << rtc::ToHex(pvi->bmiHeader.biCompression);

  return S_OK;
}

HRESULT CaptureInputPin::CheckMediaType(const CMediaType* media_type) {
  // reset the thread handle
  thread_handle_ = NULL;

  const GUID* type = media_type->Type();
  if (*type != MEDIATYPE_Video)
    return E_INVALIDARG;

  const GUID* formatType = media_type->FormatType();

  // Check for the subtypes we support
  const GUID* SubType = media_type->Subtype();
  if (SubType == NULL)
    return E_INVALIDARG;

  if (*formatType == FORMAT_VideoInfo) {
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)media_type->Format();
    if (pvi == NULL)
      return E_INVALIDARG;

    // Store the incoming width and height
    resulting_capability_.width = pvi->bmiHeader.biWidth;

    // Store the incoming height,
    // for RGB24 we assume the frame to be upside down
    if (*SubType == MEDIASUBTYPE_RGB24 && pvi->bmiHeader.biHeight > 0) {
      resulting_capability_.height = -(pvi->bmiHeader.biHeight);
    } else {
      resulting_capability_.height = abs(pvi->bmiHeader.biHeight);
    }

    RTC_LOG(LS_INFO) << "CheckMediaType width:" << pvi->bmiHeader.biWidth
                     << " height:" << pvi->bmiHeader.biHeight
                     << " Compression:0x"
                     << rtc::ToHex(pvi->bmiHeader.biCompression);

    if (*SubType == MEDIASUBTYPE_MJPG &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('M', 'J', 'P', 'G')) {
      resulting_capability_.videoType = VideoType::kMJPEG;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_I420 &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('I', '4', '2', '0')) {
      resulting_capability_.videoType = VideoType::kI420;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_YUY2 &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('Y', 'U', 'Y', '2')) {
      resulting_capability_.videoType = VideoType::kYUY2;
      ::Sleep(60);  // workaround for bad driver
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_UYVY &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('U', 'Y', 'V', 'Y')) {
      resulting_capability_.videoType = VideoType::kUYVY;
      return S_OK;  // This format is acceptable.
    }

    if (*SubType == MEDIASUBTYPE_HDYC) {
      resulting_capability_.videoType = VideoType::kUYVY;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_RGB24 &&
        pvi->bmiHeader.biCompression == BI_RGB) {
      resulting_capability_.videoType = VideoType::kRGB24;
      return S_OK;  // This format is acceptable.
    }
  }

  if (*formatType == FORMAT_VideoInfo2) {
    // VIDEOINFOHEADER2 that has dwInterlaceFlags
    VIDEOINFOHEADER2* pvi =
        reinterpret_cast<VIDEOINFOHEADER2*>(media_type->Format());
    if (pvi == NULL)
      return E_INVALIDARG;

    RTC_LOG(LS_INFO) << "CheckMediaType width:" << pvi->bmiHeader.biWidth
                     << " height:" << pvi->bmiHeader.biHeight
                     << " Compression:0x"
                     << rtc::ToHex(pvi->bmiHeader.biCompression);

    resulting_capability_.width = pvi->bmiHeader.biWidth;

    // Store the incoming height,
    // for RGB24 we assume the frame to be upside down
    if (*SubType == MEDIASUBTYPE_RGB24 && pvi->bmiHeader.biHeight > 0) {
      resulting_capability_.height = -(pvi->bmiHeader.biHeight);
    } else {
      resulting_capability_.height = abs(pvi->bmiHeader.biHeight);
    }

    if (*SubType == MEDIASUBTYPE_MJPG &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('M', 'J', 'P', 'G')) {
      resulting_capability_.videoType = VideoType::kMJPEG;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_I420 &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('I', '4', '2', '0')) {
      resulting_capability_.videoType = VideoType::kI420;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_YUY2 &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('Y', 'U', 'Y', '2')) {
      resulting_capability_.videoType = VideoType::kYUY2;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_UYVY &&
        pvi->bmiHeader.biCompression == MAKEFOURCC('U', 'Y', 'V', 'Y')) {
      resulting_capability_.videoType = VideoType::kUYVY;
      return S_OK;  // This format is acceptable.
    }

    if (*SubType == MEDIASUBTYPE_HDYC) {
      resulting_capability_.videoType = VideoType::kUYVY;
      return S_OK;  // This format is acceptable.
    }
    if (*SubType == MEDIASUBTYPE_RGB24 &&
        pvi->bmiHeader.biCompression == BI_RGB) {
      resulting_capability_.videoType = VideoType::kRGB24;
      return S_OK;  // This format is acceptable.
    }
  }

  return E_INVALIDARG;
}

HRESULT CaptureInputPin::Receive(IMediaSample* media_sample) {
  RTC_DCHECK(m_pFilter);
  CaptureSinkFilter* const filter = static_cast<CaptureSinkFilter*>(m_pFilter);

  // get the thread handle of the delivering thread inc its priority
  if (!thread_handle_) {
    thread_handle_ = GetCurrentThread();
    SetThreadPriority(thread_handle_, THREAD_PRIORITY_HIGHEST);
    rtc::SetCurrentThreadName("webrtc_video_capture");
  }

  rtc::CritScope receive_lock(&filter->receiver_lock_);
  HRESULT hr = CBaseInputPin::Receive(media_sample);

  if (SUCCEEDED(hr)) {
    const LONG length = media_sample->GetActualDataLength();
    RTC_DCHECK(length >= 0);

    unsigned char* buffer = nullptr;
    if (S_OK != media_sample->GetPointer(&buffer)) {
      return S_FALSE;
    }

    filter->ProcessCapturedFrame(buffer, static_cast<size_t>(length),
                                 resulting_capability_);
  }

  return hr;
}

// called under LockReceive
HRESULT CaptureInputPin::SetMatchingMediaType(
    const VideoCaptureCapability& capability) {
  requested_capability_ = capability;
  resulting_capability_ = VideoCaptureCapability();
  return S_OK;
}

//  ----------------------------------------------------------------------------

CaptureSinkFilter::CaptureSinkFilter(const TCHAR* object_name,
                                     IUnknown* unknown,
                                     HRESULT* hr,
                                     VideoCaptureExternal* capture_observer)
    : CBaseFilter(object_name, unknown, &filter_lock_, CLSID_SINKFILTER),
      input_pin_(new CaptureInputPin(L"VideoCaptureInputPin",
                                     this,
                                     &filter_lock_,
                                     hr,
                                     L"VideoCapture")),
      capture_observer_(capture_observer) {}

CaptureSinkFilter::~CaptureSinkFilter() {
}

int CaptureSinkFilter::GetPinCount() {
  return 1;
}

CBasePin* CaptureSinkFilter::GetPin(int index) {
  if (index == 0)
    return input_pin_.get();
  return nullptr;
}

STDMETHODIMP CaptureSinkFilter::Pause() {
  rtc::CritScope receive_lock(&receiver_lock_);  // needed?
  if (m_State == State_Stopped) {
    //  change the state, THEN activate the input pin
    m_State = State_Paused;
    if (input_pin_->IsConnected()) {
      input_pin_->Active();
    }
    if (!input_pin_->IsConnected()) {
      m_State = State_Running;
    }
  } else if (m_State == State_Running) {
    m_State = State_Paused;
  }
  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::Stop() {
  rtc::CritScope receive_lock(&receiver_lock_);

  // set the state
  m_State = State_Stopped;
  // inactivate the pins
  input_pin_->Inactive();

  return S_OK;
}

void CaptureSinkFilter::ProcessCapturedFrame(
    unsigned char* buffer,
    size_t length,
    const VideoCaptureCapability& frame_info) {
  //  we have the receiver lock
  if (m_State == State_Running)
    capture_observer_->IncomingFrame(buffer, length, frame_info);
}

STDMETHODIMP CaptureSinkFilter::QueryInterface(REFIID riid, void** ppv) {
  return GetOwner()->QueryInterface(riid, ppv);
}

STDMETHODIMP_(ULONG) CaptureSinkFilter::AddRef() {
  return GetOwner()->AddRef();
}

STDMETHODIMP_(ULONG) CaptureSinkFilter::Release() {
  return GetOwner()->Release();
}

STDMETHODIMP CaptureSinkFilter::SetMatchingMediaType(
    const VideoCaptureCapability& capability) {
  rtc::CritScope receive_lock(&receiver_lock_);  // needed?
  return input_pin_->SetMatchingMediaType(capability);
}

STDMETHODIMP CaptureSinkFilter::GetClassID(CLSID* clsid) {
  *clsid = CLSID_SINKFILTER;
  return S_OK;
}

}  // namespace videocapturemodule
}  // namespace webrtc
