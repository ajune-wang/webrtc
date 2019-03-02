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
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/string_utils.h"

#include <dvdmedia.h>  // VIDEOINFOHEADER2
#include <initguid.h>

#include <algorithm>

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
namespace {

template <class T>
class UnknownBase : public T {
 public:
  UnknownBase() {}

 protected:
  virtual ~UnknownBase() {}

  STDMETHOD_(ULONG, AddRef)() override { return ++ref_count_; }
  STDMETHOD_(ULONG, Release)() override {
    ULONG ret = --ref_count_;
    if (!ret)
      delete this;
    return ret;
  }

 private:
  ULONG ref_count_ = 0;
};

// Simple enumeration implementation that enumerates over a single pin :-/
class EnumPins : public UnknownBase<IEnumPins> {
 public:
  EnumPins(IPin* pin) : UnknownBase<IEnumPins>(), pin_(pin) {}

 private:
  ~EnumPins() override {}

  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
    if (riid == IID_IUnknown || riid == IID_IEnumPins) {
      *ppv = static_cast<IEnumPins*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHOD(Clone)(IEnumPins** pins) {
    RTC_DCHECK(false);
    return E_NOTIMPL;
  }

  STDMETHOD(Next)(ULONG count, IPin** pins, ULONG* fetched) {
    RTC_DCHECK(count > 0);
    RTC_DCHECK(pins);
    // fetched may be NULL.

    if (pos_ > 0) {
      if (fetched)
        *fetched = 0;
      return S_FALSE;
    }

    ++pos_;
    pins[0] = pin_.get();
    pins[0]->AddRef();
    if (fetched)
      *fetched = 1;

    return count == 1 ? S_OK : S_FALSE;
  }

  STDMETHOD(Skip)(ULONG count) {
    RTC_DCHECK(false);
    return E_NOTIMPL;
  }

  STDMETHOD(Reset)() {
    pos_ = 0;
    return S_OK;
  }

  rtc::scoped_refptr<IPin> pin_;
  int pos_ = 0;
};

void ClearMediaType(AM_MEDIA_TYPE* media_type) {
  if (!media_type)
    return;
  FreeMediaType(*media_type);
  CoTaskMemFree(media_type);
}

bool MediaTypePartialMatch(const AM_MEDIA_TYPE& a, const AM_MEDIA_TYPE& b) {
  if (b.majortype != GUID_NULL && a.majortype != b.majortype)
    return false;

  if (b.subtype != GUID_NULL && a.subtype != b.subtype)
    return false;

  if (b.formattype != GUID_NULL) {
    // if the format block is specified then it must match exactly
    if (a.formattype != b.formattype)
      return false;

    if (a.cbFormat != b.cbFormat)
      return false;

    if (a.cbFormat != 0 && memcmp(a.pbFormat, b.pbFormat, a.cbFormat) != 0)
      return false;
  }

  return true;
}

bool IsMediaTypePartiallySpecified(const AM_MEDIA_TYPE& type) {
  if (type.majortype == GUID_NULL || type.formattype == GUID_NULL) {
    return true;
  }
  return false;
}

BYTE* AllocMediaTypeFormatBuffer(AM_MEDIA_TYPE* media_type, ULONG length) {
  RTC_DCHECK(length);
  if (media_type->cbFormat == length)
    return media_type->pbFormat;

  BYTE* buffer = static_cast<BYTE*>(CoTaskMemAlloc(length));
  if (!buffer)
    return nullptr;

  if (media_type->pbFormat) {
    RTC_DCHECK(media_type->cbFormat);
    CoTaskMemFree(media_type->pbFormat);
    media_type->pbFormat = nullptr;
  }

  media_type->cbFormat = length;
  media_type->pbFormat = buffer;
  return buffer;
}

void GetSampleProperties(IMediaSample* sample, AM_SAMPLE2_PROPERTIES* props) {
  IMediaSample2* sample2 = nullptr;
  if (SUCCEEDED(sample->QueryInterface(IID_IMediaSample2,
                                       reinterpret_cast<void**>(&sample2)))) {
    sample2->GetProperties(sizeof(*props), reinterpret_cast<BYTE*>(props));
    sample2->Release();
    return;
  }

  //  Get the properties the hard way.
  props->cbData = sizeof(*props);
  props->dwTypeSpecificFlags = 0;
  props->dwStreamId = AM_STREAM_MEDIA;
  props->dwSampleFlags = 0;

  if (sample->IsDiscontinuity() == S_OK)
    props->dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;

  if (sample->IsPreroll() == S_OK)
    props->dwSampleFlags |= AM_SAMPLE_PREROLL;

  if (sample->IsSyncPoint() == S_OK)
    props->dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;

  if (SUCCEEDED(sample->GetTime(&props->tStart, &props->tStop)))
    props->dwSampleFlags |= AM_SAMPLE_TIMEVALID | AM_SAMPLE_STOPVALID;

  if (sample->GetMediaType(&props->pMediaType) == S_OK)
    props->dwSampleFlags |= AM_SAMPLE_TYPECHANGED;

  sample->GetPointer(&props->pbBuffer);
  props->lActual = sample->GetActualDataLength();
  props->cbBuffer = sample->GetSize();
}

class MediaTypesEnum : public UnknownBase<IEnumMediaTypes> {
 public:
  MediaTypesEnum(const VideoCaptureCapability& capability)
      : UnknownBase<IEnumMediaTypes>(), capability_(capability) {}

 private:
  ~MediaTypesEnum() override {}

  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
    if (riid == IID_IUnknown || riid == IID_IEnumMediaTypes) {
      *ppv = static_cast<IEnumMediaTypes*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  // IEnumMediaTypes
  STDMETHOD(Clone)(IEnumMediaTypes** pins) {
    RTC_DCHECK(false);
    return E_NOTIMPL;
  }

  STDMETHOD(Next)(ULONG count, AM_MEDIA_TYPE** types, ULONG* fetched) {
    RTC_DCHECK(count > 0);
    RTC_DCHECK(types);
    // fetched may be NULL.
    if (fetched)
      *fetched = 0;

    // Note, must match switch statement below.
    constexpr int kNumTypes = 5;

    for (ULONG i = 0; i < count && pos_ < kNumTypes; ++i) {
      AM_MEDIA_TYPE* media_type = reinterpret_cast<AM_MEDIA_TYPE*>(
          CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
      ZeroMemory(media_type, sizeof(*media_type));
      types[i] = media_type;
      VIDEOINFOHEADER* vih = reinterpret_cast<VIDEOINFOHEADER*>(
          AllocMediaTypeFormatBuffer(media_type, sizeof(VIDEOINFOHEADER)));
      ZeroMemory(vih, sizeof(*vih));
      vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      vih->bmiHeader.biPlanes = 1;
      vih->bmiHeader.biClrImportant = 0;
      vih->bmiHeader.biClrUsed = 0;
      if (capability_.maxFPS != 0)
        vih->AvgTimePerFrame = 10000000 / capability_.maxFPS;

      SetRectEmpty(&vih->rcSource);  // we want the whole image area rendered.
      SetRectEmpty(&vih->rcTarget);  // no particular destination rectangle

      media_type->majortype = MEDIATYPE_Video;
      media_type->formattype = FORMAT_VideoInfo;
      media_type->bTemporalCompression = FALSE;

      switch (pos_++) {
        case 0:
          // Note: The previous implementation had a bug that caused this
          // format to always be omitted. Perhaps including it now will have
          // a sideffect.
          vih->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
          vih->bmiHeader.biBitCount = 12;  // bit per pixel
          media_type->subtype = MEDIASUBTYPE_I420;
          break;
        case 1:
          vih->bmiHeader.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
          vih->bmiHeader.biBitCount = 16;  // bit per pixel
          media_type->subtype = MEDIASUBTYPE_YUY2;
          break;
        case 2:
          vih->bmiHeader.biCompression = BI_RGB;
          vih->bmiHeader.biBitCount = 24;  // bit per pixel
          media_type->subtype = MEDIASUBTYPE_RGB24;
          break;
        case 3:
          vih->bmiHeader.biCompression = MAKEFOURCC('U', 'Y', 'V', 'Y');
          vih->bmiHeader.biBitCount = 16;  // bit per pixel
          media_type->subtype = MEDIASUBTYPE_UYVY;
          break;
        case 4:
          vih->bmiHeader.biCompression = MAKEFOURCC('M', 'J', 'P', 'G');
          vih->bmiHeader.biBitCount = 12;  // bit per pixel
          media_type->subtype = MEDIASUBTYPE_MJPG;
          break;
        default:
          RTC_NOTREACHED();
          break;
      }

      vih->bmiHeader.biWidth = capability_.width;
      vih->bmiHeader.biHeight = capability_.height;
      vih->bmiHeader.biSizeImage = ((vih->bmiHeader.biBitCount / 4) *
                                    capability_.height * capability_.width) /
                                   2;

      RTC_DCHECK(vih->bmiHeader.biSizeImage);
      media_type->lSampleSize = vih->bmiHeader.biSizeImage;
      media_type->bFixedSizeSamples = true;
      if (fetched)
        ++(*fetched);
    }
    RTC_DCHECK(pos_ <= kNumTypes);
    return pos_ == kNumTypes ? S_FALSE : S_OK;
  }

  STDMETHOD(Skip)(ULONG count) {
    RTC_DCHECK(false);
    return E_NOTIMPL;
  }

  STDMETHOD(Reset)() {
    pos_ = 0;
    return S_OK;
  }

  int pos_ = 0;
  const VideoCaptureCapability capability_;
};

}  // namespace

CaptureInputPin::CaptureInputPin(LPCTSTR object_name,
                                 CaptureSinkFilter* filter,
                                 CCritSec* lock,
                                 HRESULT* hr,
                                 LPCWSTR name)
    : CBaseInputPin(object_name, filter, lock, hr, name) {
  capture_checker_.DetachFromThread();
  // No reference held to avoid circular references.
  info_.pFilter = filter;
  info_.dir = PINDIR_INPUT;
  if (name)
    lstrcpynW(info_.achName, name, arraysize(info_.achName));
}

CaptureInputPin::~CaptureInputPin() {
  RTC_DCHECK_RUN_ON(&main_checker_);
}

bool CaptureInputPin::IsSupportedMediaType(
    const AM_MEDIA_TYPE* media_type,
    VideoCaptureCapability* capability) const {
  RTC_DCHECK(capability);
  if (!media_type || media_type->majortype != MEDIATYPE_Video ||
      !media_type->pbFormat) {
    return false;
  }

  const BITMAPINFOHEADER* bih = nullptr;
  if (media_type->formattype == FORMAT_VideoInfo) {
    bih = &reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat)->bmiHeader;
  } else if (media_type->formattype != FORMAT_VideoInfo2) {
    bih = &reinterpret_cast<VIDEOINFOHEADER2*>(media_type->pbFormat)->bmiHeader;
  } else {
    return false;
  }

  RTC_LOG(LS_INFO) << "IsSupportedMediaType width:" << bih->biWidth
                   << " height:" << bih->biHeight << " Compression:0x"
                   << rtc::ToHex(bih->biCompression);

  const GUID& sub_type = media_type->subtype;
  if (sub_type == MEDIASUBTYPE_MJPG &&
      bih->biCompression == MAKEFOURCC('M', 'J', 'P', 'G')) {
    capability->videoType = VideoType::kMJPEG;
  } else if (sub_type == MEDIASUBTYPE_I420 &&
             bih->biCompression == MAKEFOURCC('I', '4', '2', '0')) {
    capability->videoType = VideoType::kI420;
  } else if (sub_type == MEDIASUBTYPE_YUY2 &&
             bih->biCompression == MAKEFOURCC('Y', 'U', 'Y', '2')) {
    capability->videoType = VideoType::kYUY2;
  } else if (sub_type == MEDIASUBTYPE_UYVY &&
             bih->biCompression == MAKEFOURCC('U', 'Y', 'V', 'Y')) {
    capability->videoType = VideoType::kUYVY;
  } else if (sub_type == MEDIASUBTYPE_HDYC) {
    capability->videoType = VideoType::kUYVY;
  } else if (sub_type == MEDIASUBTYPE_RGB24 && bih->biCompression == BI_RGB) {
    capability->videoType = VideoType::kRGB24;
  } else {
    return false;
  }

  // Store the incoming width and height
  capability->width = bih->biWidth;

  // Store the incoming height,
  // for RGB24 we assume the frame to be upside down
  if (sub_type == MEDIASUBTYPE_RGB24 && bih->biHeight > 0) {
    capability->height = -(bih->biHeight);
  } else {
    capability->height = abs(bih->biHeight);
  }

  return true;
}

HRESULT CaptureInputPin::SetMatchingMediaType(
    const VideoCaptureCapability& capability) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  requested_capability_ = capability;
  resulting_capability_ = VideoCaptureCapability();
  return S_OK;
}

HRESULT CaptureInputPin::CheckMediaType(const CMediaType*) {
  RTC_NOTREACHED();
  return E_UNEXPECTED;
}

HRESULT CaptureInputPin::AgreeMediaType(IPin* receive_pin,
                                        const AM_MEDIA_TYPE* media_type) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  RTC_DCHECK(receive_pin);
  RTC_DCHECK(media_type);

  // if the media type is fully specified then use that.
  if (!IsMediaTypePartiallySpecified(*media_type))
    return AttemptConnection(receive_pin, media_type);

  HRESULT hr_failure = VFW_E_NO_ACCEPTABLE_TYPES;

  for (int i = 0; i < 2; i++) {
    IEnumMediaTypes* types = nullptr;
    if (i == 0) {
      // First time around, try types from receive_pin.
      receive_pin->EnumMediaTypes(&types);
    } else {
      // Then try ours.
      EnumMediaTypes(&types);
    }

    if (types) {
      HRESULT hr = TryMediaTypes(receive_pin, media_type, types);
      types->Release();
      if (SUCCEEDED(hr))
        return S_OK;

      // try to remember specific error codes if there are any
      if (hr != E_FAIL && hr != E_INVALIDARG && hr != VFW_E_TYPE_NOT_ACCEPTED)
        hr_failure = hr;
    }
  }

  return hr_failure;
}

HRESULT CaptureInputPin::AttemptConnection(IPin* receive_pin,
                                           const AM_MEDIA_TYPE* media_type) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  // Check that the connection is valid  -- need to do this for every
  // connect attempt since BreakConnect will undo it.
  HRESULT hr = CheckDirection(receive_pin);
  if (FAILED(hr))
    return hr;

  // The caller should hold the filter lock becasue this function
  // uses m_Connected.  The caller should also hold the filter lock
  // because this function calls SetMediaType(), IsStopped()
  // ASSERT(CritCheckIn(m_pLock));

  if (IsSupportedMediaType(media_type, &resulting_capability_)) {
    /*  Make ourselves look connected otherwise ReceiveConnection
        may not be able to complete the connection
    */
    m_Connected = receive_pin;
    m_Connected->AddRef();
    hr = m_mt.Set(*media_type);
    if (SUCCEEDED(hr)) {
      // See if the other pin will accept this type.
      hr = receive_pin->ReceiveConnection(static_cast<IPin*>(this), media_type);
      if (SUCCEEDED(hr))
        return S_OK;
    }
  } else {
    hr = VFW_E_TYPE_NOT_ACCEPTED;
  }

  ClearAllocator(true);

  // We didnt succeed, release reference if we hold it.
  if (m_Connected) {
    m_Connected->Release();
    m_Connected = nullptr;
  }

  return hr;
}

void CaptureInputPin::ClearAllocator(bool decommit) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  if (!allocator_)
    return;
  if (decommit)
    allocator_->Decommit();
  allocator_ = nullptr;
}

HRESULT CaptureInputPin::CheckDirection(IPin* pin) const {
  RTC_DCHECK_RUN_ON(&main_checker_);
  PIN_DIRECTION pd;
  pin->QueryDirection(&pd);
  // Fairly basic check, make sure we don't pair input with input etc.
  return pd == info_.dir ? VFW_E_INVALID_DIRECTION : S_OK;
}

HRESULT CaptureInputPin::TryMediaTypes(IPin* receive_pin,
                                       const AM_MEDIA_TYPE* media_type,
                                       IEnumMediaTypes* types) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  HRESULT hr = VFW_E_NO_ACCEPTABLE_TYPES;
  while (hr != S_OK) {
    ULONG fetched = 0;
    AM_MEDIA_TYPE* this_type = nullptr;
    hr = types->Next(1, &this_type, &fetched);
    if (hr != S_OK)
      return VFW_E_NO_ACCEPTABLE_TYPES;

    if (MediaTypePartialMatch(*this_type, *media_type)) {
      hr = AttemptConnection(receive_pin, this_type);
    } else {
      hr = VFW_E_NO_ACCEPTABLE_TYPES;
    }

    ClearMediaType(this_type);
  }

  return hr;
}

STDMETHODIMP CaptureInputPin::QueryInterface(REFIID riid, void** ppv) {
  (*ppv) = nullptr;
  if (riid == IID_IUnknown || riid == IID_IMemInputPin) {
    *ppv = static_cast<IMemInputPin*>(this);
  } else if (riid == IID_IPin) {
    *ppv = static_cast<IPin*>(this);
  } else if (riid == IID_IQualityControl) {
    *ppv = static_cast<IQualityControl*>(this);
  }

  if (!(*ppv))
    return E_NOINTERFACE;

  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) CaptureInputPin::AddRef() {
  return CBaseInputPin::AddRef();
}

STDMETHODIMP_(ULONG) CaptureInputPin::Release() {
  return CBaseInputPin::Release();
}

STDMETHODIMP CaptureInputPin::Connect(IPin* receive_pin,
                                      const AM_MEDIA_TYPE* media_type) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  if (!media_type || !receive_pin)
    return E_POINTER;

  CAutoLock cObjectLock(m_pLock);

  if (m_Connected)
    return VFW_E_ALREADY_CONNECTED;

  /* See if the filter is active */
  if (!m_pFilter->IsStopped() && !m_bCanReconnectWhenActive)
    return VFW_E_NOT_STOPPED;

  // Find a mutually agreeable media type -
  // Pass in the template media type. If this is partially specified,
  // each of the enumerated media types will need to be checked against
  // it. If it is non-null and fully specified, we will just try to connect
  // with this.

  return AgreeMediaType(receive_pin, media_type);
}

STDMETHODIMP CaptureInputPin::ReceiveConnection(
    IPin* connector,
    const AM_MEDIA_TYPE* media_type) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  CAutoLock cObjectLock(m_pLock);
  if (m_Connected)
    return VFW_E_ALREADY_CONNECTED;

  /* See if the filter is active */
  if (!m_pFilter->IsStopped() && !m_bCanReconnectWhenActive)
    return VFW_E_NOT_STOPPED;

  HRESULT hr = CheckDirection(connector);
  if (FAILED(hr))
    return hr;

  if (!IsSupportedMediaType(media_type, &resulting_capability_))
    return VFW_E_TYPE_NOT_ACCEPTED;

  // Complete the connection

  m_Connected = connector;
  m_Connected->AddRef();
  hr = m_mt.Set(*media_type);
  if (SUCCEEDED(hr))
    return S_OK;

  m_Connected->Release();
  m_Connected = NULL;

  return hr;
}

STDMETHODIMP CaptureInputPin::Disconnect() {
  RTC_DCHECK_RUN_ON(&main_checker_);
  if (!m_pFilter->IsStopped())
    return VFW_E_NOT_STOPPED;

  if (!m_Connected)
    return S_FALSE;

  ClearAllocator(true);
  m_Connected->Release();
  m_Connected = NULL;
  return S_OK;
}

STDMETHODIMP CaptureInputPin::ConnectedTo(IPin** pin) {
  RTC_DCHECK_RUN_ON(&main_checker_);

  if (!m_Connected)
    return VFW_E_NOT_CONNECTED;

  *pin = m_Connected;
  m_Connected->AddRef();

  return S_OK;
}

STDMETHODIMP CaptureInputPin::ConnectionMediaType(AM_MEDIA_TYPE* media_type) {
  RTC_DCHECK_RUN_ON(&main_checker_);

  if (!IsConnected())
    return VFW_E_NOT_CONNECTED;

  CopyMediaType(media_type, &m_mt);

  return S_OK;
}

STDMETHODIMP CaptureInputPin::QueryPinInfo(PIN_INFO* info) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  *info = info_;
  if (info_.pFilter)
    info_.pFilter->AddRef();
  return S_OK;
}

STDMETHODIMP CaptureInputPin::QueryDirection(PIN_DIRECTION* pin_dir) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  *pin_dir = info_.dir;
  return S_OK;
}

STDMETHODIMP CaptureInputPin::QueryId(LPWSTR* id) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  return AMGetWideString(info_.achName, id);
}

STDMETHODIMP CaptureInputPin::QueryAccept(const AM_MEDIA_TYPE* media_type) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  VideoCaptureCapability capability(resulting_capability_);
  return IsSupportedMediaType(media_type, &capability) ? S_FALSE : S_OK;
}

STDMETHODIMP CaptureInputPin::EnumMediaTypes(IEnumMediaTypes** types) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  *types = new MediaTypesEnum(requested_capability_);
  (*types)->AddRef();
  return S_OK;
}

STDMETHODIMP CaptureInputPin::QueryInternalConnections(IPin** pins,
                                                       ULONG* count) {
  return E_NOTIMPL;
}

STDMETHODIMP CaptureInputPin::EndOfStream() {
  return S_OK;
}

STDMETHODIMP CaptureInputPin::BeginFlush() {
  RTC_DCHECK_RUN_ON(&main_checker_);
  CAutoLock lck(m_pLock);
  RTC_DCHECK(!m_bFlushing);
  m_bFlushing = TRUE;
  return S_OK;
}

STDMETHODIMP CaptureInputPin::EndFlush() {
  RTC_DCHECK_RUN_ON(&main_checker_);
  CAutoLock lck(m_pLock);
  RTC_DCHECK(m_bFlushing);
  m_bFlushing = FALSE;
  m_bRunTimeError = FALSE;
  return S_OK;
}

STDMETHODIMP CaptureInputPin::NewSegment(REFERENCE_TIME start,
                                         REFERENCE_TIME stop,
                                         double rate) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  m_tStart = start;
  m_tStop = stop;
  m_dRate = rate;
  return S_OK;
}

STDMETHODIMP CaptureInputPin::GetAllocator(IMemAllocator** allocator) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  if (allocator_ == nullptr) {
    HRESULT hr = CreateMemoryAllocator(allocator);
    if (FAILED(hr))
      return hr;
    allocator_.swap(allocator);
  }
  *allocator = allocator_;
  allocator_->AddRef();
  return S_OK;
}

STDMETHODIMP CaptureInputPin::NotifyAllocator(IMemAllocator* allocator,
                                              BOOL read_only) {
  RTC_DCHECK_RUN_ON(&main_checker_);
  allocator_.swap(&allocator);
  if (allocator_)
    allocator_->AddRef();
  if (allocator)
    allocator->Release();

  m_bReadOnly = (BYTE)read_only;
  return S_OK;
}

STDMETHODIMP CaptureInputPin::GetAllocatorRequirements(
    ALLOCATOR_PROPERTIES* props) {
  return E_NOTIMPL;
}

STDMETHODIMP CaptureInputPin::Receive(IMediaSample* media_sample) {
  RTC_DCHECK_RUN_ON(&capture_checker_);

  CaptureSinkFilter* const filter = static_cast<CaptureSinkFilter*>(m_pFilter);

  // TODO(tommi): Determine what the expected state is here.
  // Quite possibly we don't need to check all of these (possibly not any).
  // Since we're running on a different thread than these properties are usually
  // checked on, it would be good to not have to do that (and synchronize).
  if (filter->IsStopped())
    return VFW_E_WRONG_STATE;

  if (m_bFlushing)
    return S_FALSE;

  if (m_bRunTimeError)
    return VFW_E_RUNTIME_ERROR;

  // get the thread handle of the delivering thread inc its priority
  // TODO(tommi): Figure out some other way of setting the thread name.
  if (!thread_handle_) {
    thread_handle_ = GetCurrentThread();
    // TODO(tommi): Necessary?
    SetThreadPriority(thread_handle_, THREAD_PRIORITY_HIGHEST);
    rtc::SetCurrentThreadName("webrtc_video_capture");
  }

  GetSampleProperties(media_sample, &m_SampleProps);
  // Has the format changed in this sample?
  if (m_SampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED) {
    // Check the derived class accepts the new format.
    // This shouldn't fail as the source must call QueryAccept first.
    // TODO(tommi): ^^^ Should we rather RTC_CHECK than error handle?
    // Note: This will modify resulting_capability_!
    if (!IsSupportedMediaType(m_SampleProps.pMediaType,
                              &resulting_capability_)) {
      // Raise a runtime error if we fail the media type */
      // TODO(tommi): This breaks threading rules. Presumably if we return
      // an error, a notification of some sort will happen on the
      // control thread?
      m_bRunTimeError = TRUE;
      EndOfStream();
      m_pFilter->NotifyEvent(EC_ERRORABORT, VFW_E_TYPE_NOT_ACCEPTED, 0);
      return VFW_E_INVALIDMEDIATYPE;
    }
  }

  const LONG length = media_sample->GetActualDataLength();
  RTC_DCHECK(length >= 0);

  unsigned char* buffer = nullptr;
  if (S_OK != media_sample->GetPointer(&buffer))
    return S_FALSE;

  // TODO(tommi): See if we need this lock. Move it into ProcessCapturedFrame.
  filter->ProcessCapturedFrame(buffer, static_cast<size_t>(length),
                               resulting_capability_);

  return S_OK;
}

STDMETHODIMP CaptureInputPin::ReceiveMultiple(IMediaSample** samples,
                                              long count,
                                              long* processed) {
  HRESULT hr = S_OK;
  *processed = 0;
  while (count-- > 0) {
    hr = Receive(samples[*processed]);
    if (hr != S_OK)
      break;
    ++(*processed);
  }
  return hr;
}

STDMETHODIMP CaptureInputPin::ReceiveCanBlock() {
  const int pins = m_pFilter->GetPinCount();
  int cOutputPins = 0;
  for (int c = 0; c < pins; c++) {
    CBasePin* pPin = m_pFilter->GetPin(c);
    if (NULL == pPin)
      break;

    PIN_DIRECTION pd;
    HRESULT hr = pPin->QueryDirection(&pd);
    if (FAILED(hr))
      return hr;

    if (pd == PINDIR_OUTPUT) {
      IPin* pConnected;
      hr = pPin->ConnectedTo(&pConnected);
      if (SUCCEEDED(hr)) {
        ASSERT(pConnected != NULL);
        cOutputPins++;
        IMemInputPin* pInputPin;
        hr = pConnected->QueryInterface(IID_IMemInputPin, (void**)&pInputPin);
        pConnected->Release();
        if (SUCCEEDED(hr)) {
          hr = pInputPin->ReceiveCanBlock();
          pInputPin->Release();
          if (hr != S_FALSE)
            return S_OK;
        } else {
          /*  There's a transport we don't understand here */
          return S_OK;
        }
      }
    }
  }
  return cOutputPins == 0 ? S_OK : S_FALSE;
}

STDMETHODIMP CaptureInputPin::Notify(IBaseFilter* self, Quality q) {
  return S_OK;
}

STDMETHODIMP CaptureInputPin::SetSink(IQualityControl* qc) {
  // No reference held to avoid circular references.
  m_pQSink = qc;
  return S_OK;
}

//  ----------------------------------------------------------------------------

CaptureSinkFilter::CaptureSinkFilter(const TCHAR* object_name,
                                     IUnknown* unknown,
                                     HRESULT* hr,
                                     VideoCaptureExternal* capture_observer)
    : CBaseFilter(object_name, unknown, &lock_, CLSID_SINKFILTER),
      input_pin_(new CaptureInputPin(L"VideoCaptureInputPin",
                                     this,
                                     &lock_,
                                     hr,
                                     L"VideoCapture")),
      capture_observer_(capture_observer) {}

CaptureSinkFilter::~CaptureSinkFilter() {}

HRESULT CaptureSinkFilter::SetMatchingMediaType(
    const VideoCaptureCapability& capability) {
  // Called on the same thread as capture is started on.
  rtc::CritScope receive_lock(&receiver_lock_);  // needed?
  return input_pin_->SetMatchingMediaType(capability);
}

int CaptureSinkFilter::GetPinCount() {
  RTC_CHECK(false);
  return 1;
}

CBasePin* CaptureSinkFilter::GetPin(int index) {
  RTC_CHECK(false);
  if (index == 0)
    return input_pin_.get();
  return nullptr;
}

HRESULT CaptureSinkFilter::StreamTime(CRefTime& ref_time) {
  RTC_CHECK(false);  // this method doesn't appear to be called (or needed).
  return CBaseFilter::StreamTime(ref_time);
}

STDMETHODIMP CaptureSinkFilter::GetState(DWORD msecs, FILTER_STATE* state) {
  *state = m_State;
  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::SetSyncSource(IReferenceClock* clock) {
  // Always called on the same thread?
  CAutoLock cObjectLock(m_pLock);

  if (clock)
    clock->AddRef();
  if (m_pClock)
    m_pClock->Release();

  m_pClock = clock;

  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::GetSyncSource(IReferenceClock** clock) {
  // Always called on the same thread?
  CAutoLock cObjectLock(m_pLock);

  if (m_pClock)
    m_pClock->AddRef();
  *clock = m_pClock;
  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::Pause() {
  rtc::CritScope receive_lock(&receiver_lock_);  // needed?
  if (m_State == State_Stopped) {
    //  change the state, THEN activate the input pin
    m_State = State_Paused;
    if (input_pin_->IsConnected())
      input_pin_->Active();
    if (!input_pin_->IsConnected())
      m_State = State_Running;
  } else if (m_State == State_Running) {
    m_State = State_Paused;
  }
  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::Run(REFERENCE_TIME tStart) {
  CAutoLock cObjectLock(&lock_);

  m_tStart = tStart;
  if (m_State == State_Stopped)
    Pause();

  m_State = State_Running;

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

STDMETHODIMP CaptureSinkFilter::EnumPins(IEnumPins** pins) {
  *pins = new class EnumPins(input_pin_.get());
  (*pins)->AddRef();
  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::FindPin(LPCWSTR id, IPin** pin) {
  if (lstrcmpW(input_pin_->Name(), id) == 0) {
    *pin = input_pin_.get();
    (*pin)->AddRef();
    return S_OK;
  }

  return VFW_E_NOT_FOUND;
}

STDMETHODIMP CaptureSinkFilter::QueryFilterInfo(FILTER_INFO* info) {
  ZeroMemory(info, sizeof(*info));
  if (m_pName)
    lstrcpyW(info->achName, m_pName);
  info->pGraph = m_pGraph;
  if (m_pGraph)
    m_pGraph->AddRef();
  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::JoinFilterGraph(IFilterGraph* graph,
                                                LPCWSTR name) {
  CAutoLock cObjectLock(m_pLock);
  // Note, since a reference to the filter is held by the graph manager,
  // filters must not hold a reference to the graph. If they would, we'd have
  // a circular reference. Instead, a pointer to the graph can be held without
  // reference. See documentation for IBaseFilter::JoinFilterGraph for more.
  m_pGraph = graph;  // No AddRef().
  m_pSink = nullptr;
  if (m_pGraph) {
    HRESULT hr = m_pGraph->QueryInterface(IID_IMediaEventSink,
                                          reinterpret_cast<void**>(&m_pSink));
    if (SUCCEEDED(hr)) {
      // make sure we don't hold on to the reference we got.
      // Note that this assumes the same object identity, but so be it.
      m_pSink->Release();
    }
  }

  if (m_pName) {
    delete[] m_pName;
    m_pName = nullptr;
  }

  if (name) {
    size_t len = lstrlenW(name);
    m_pName = new WCHAR[len + 1];
    lstrcpyW(m_pName, name);
  }

  return S_OK;
}

STDMETHODIMP CaptureSinkFilter::QueryVendorInfo(LPWSTR* vendor_info) {
  return E_NOTIMPL;
}

void CaptureSinkFilter::ProcessCapturedFrame(
    unsigned char* buffer,
    size_t length,
    const VideoCaptureCapability& frame_info) {
  // Called on the capture thread.
  // TODO(tommi): Do we need this lock? Is checking the state necessary?
  rtc::CritScope receive_lock(&receiver_lock_);
  if (m_State == State_Running)
    capture_observer_->IncomingFrame(buffer, length, frame_info);
}

STDMETHODIMP CaptureSinkFilter::QueryInterface(REFIID riid, void** ppv) {
  if (riid == IID_IUnknown || riid == IID_IPersist || riid == IID_IBaseFilter) {
    *ppv = static_cast<IBaseFilter*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CaptureSinkFilter::AddRef() {
  return GetOwner()->AddRef();
}

STDMETHODIMP_(ULONG) CaptureSinkFilter::Release() {
  return GetOwner()->Release();
}

STDMETHODIMP CaptureSinkFilter::GetClassID(CLSID* clsid) {
  *clsid = CLSID_SINKFILTER;
  return S_OK;
}

}  // namespace videocapturemodule
}  // namespace webrtc
