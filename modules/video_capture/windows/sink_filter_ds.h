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
#include "rtc_base/thread_checker.h"

namespace webrtc {
namespace videocapturemodule {
// forward declaration

class CaptureSinkFilter;
/**
 *     input pin for camera input
 *
 */
// Implement IMemInputPin, IPin, IQualityControl.
class CaptureInputPin : public CBaseInputPin {
 public:
  CaptureInputPin(LPCTSTR object_name,
                  CaptureSinkFilter* filter,
                  CCritSec* lock,
                  HRESULT* hr,
                  LPCWSTR name);
  ~CaptureInputPin() override;

  bool IsSupportedMediaType(const AM_MEDIA_TYPE* media_type,
                            VideoCaptureCapability* capability) const;
  HRESULT SetMatchingMediaType(const VideoCaptureCapability& capability);

  // Called by the filter when the filter starts.
  void OnActive();
  // Called by the filter when the filter is stopped.
  void OnInactive();

 private:
  // TODO(tommi): Delete after stop inheriting from CBaseInputPin.
  HRESULT CheckMediaType(const CMediaType*) override;

  HRESULT AgreeMediaType(IPin* receive_pin, const AM_MEDIA_TYPE* media_type);
  HRESULT AttemptConnection(IPin* receive_pin, const AM_MEDIA_TYPE* media_type);
  void ClearAllocator(bool decommit);
  HRESULT CheckDirection(IPin* pin) const;
  HRESULT TryMediaTypes(IPin* receive_pin,
                        const AM_MEDIA_TYPE* media_type,
                        IEnumMediaTypes* types);
  // IUnknown
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
  STDMETHOD_(ULONG, AddRef)() override;
  STDMETHOD_(ULONG, Release)() override;

  // IPin
  STDMETHOD(Connect)
  (IPin* receive_pin, const AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(ReceiveConnection)
  (IPin* connector, const AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(Disconnect)() override;
  STDMETHOD(ConnectedTo)(IPin** pin) override;
  STDMETHOD(ConnectionMediaType)(AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(QueryPinInfo)(PIN_INFO* info) override;
  STDMETHOD(QueryDirection)(PIN_DIRECTION* pin_dir) override;
  STDMETHOD(QueryId)(LPWSTR* id) override;
  STDMETHOD(QueryAccept)(const AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(EnumMediaTypes)(IEnumMediaTypes** types) override;
  STDMETHOD(QueryInternalConnections)(IPin** pins, ULONG* count) override;
  STDMETHOD(EndOfStream)() override;
  STDMETHOD(BeginFlush)() override;
  STDMETHOD(EndFlush)() override;
  STDMETHOD(NewSegment)
  (REFERENCE_TIME start, REFERENCE_TIME stop, double rate) override;

  // IMemInputPin
  STDMETHOD(GetAllocator)(IMemAllocator** allocator) override;
  STDMETHOD(NotifyAllocator)(IMemAllocator* allocator, BOOL read_only) override;
  STDMETHOD(GetAllocatorRequirements)(ALLOCATOR_PROPERTIES* props) override;
  STDMETHOD(Receive)(IMediaSample* sample) override;
  STDMETHOD(ReceiveMultiple)
  (IMediaSample** samples, long count, long* processed) override;
  STDMETHOD(ReceiveCanBlock)() override;

  // IQualityControl
  STDMETHOD(Notify)(IBaseFilter* self, Quality q) override;
  STDMETHOD(SetSink)(IQualityControl* qc) override;

 private:
  rtc::ThreadChecker main_checker_;
  rtc::ThreadChecker capture_checker_;
  // TODO(tommi): Figure out and document access.
  VideoCaptureCapability requested_capability_;
  VideoCaptureCapability resulting_capability_;
  DWORD capture_thread_id_ = 0;
  rtc::scoped_refptr<IMemAllocator> allocator_ RTC_GUARDED_BY(main_checker_);
  // Holds a referenceless pointer to the owning filter, the name and
  // direction of the pin. The filter pointer can be considered const.
  PIN_INFO info_ = {};
};

// Implement IBaseFilter (including IPersist and IMediaFilter).
class CaptureSinkFilter : public CBaseFilter {
 public:
  CaptureSinkFilter(LPCTSTR object_name,
                    IUnknown* unknown,
                    HRESULT* hr,
                    VideoCaptureExternal* capture_observer);
  ~CaptureSinkFilter() override;

  HRESULT SetMatchingMediaType(const VideoCaptureCapability& capability);

  // Called on the capture thread.
  void ProcessCapturedFrame(unsigned char* buffer,
                            size_t length,
                            const VideoCaptureCapability& frame_info);

  //  CBaseFilter methods - TODO: Delete.
  int GetPinCount() override;
  CBasePin* GetPin(IN int Index) override;
  HRESULT StreamTime(CRefTime& ref_time) override;

  //  IUnknown
  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
  STDMETHOD_(ULONG, AddRef)() override;
  STDMETHOD_(ULONG, Release)() override;

  // IPersist
  STDMETHOD(GetClassID)(CLSID* clsid) override;

  // IMediaFilter.
  STDMETHOD(GetState)(DWORD msecs, FILTER_STATE* state) override;
  STDMETHOD(SetSyncSource)(IReferenceClock* clock) override;
  STDMETHOD(GetSyncSource)(IReferenceClock** clock) override;
  STDMETHOD(Pause)() override;
  STDMETHOD(Run)(REFERENCE_TIME start) override;
  STDMETHOD(Stop)() override;

  // IBaseFilter
  STDMETHOD(EnumPins)(IEnumPins** pins) override;
  STDMETHOD(FindPin)(LPCWSTR id, IPin** pin) override;
  STDMETHOD(QueryFilterInfo)(FILTER_INFO* info) override;
  STDMETHOD(JoinFilterGraph)(IFilterGraph* graph, LPCWSTR name) override;
  STDMETHOD(QueryVendorInfo)(LPWSTR* vendor_info) override;

 private:
  // TODO(tommi): Is this lock needed?
  const rtc::CriticalSection receiver_lock_;
  CCritSec lock_;
  const std::unique_ptr<CaptureInputPin> input_pin_;
  VideoCaptureExternal* capture_observer_;
};
}  // namespace videocapturemodule
}  // namespace webrtc
#endif  // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_
