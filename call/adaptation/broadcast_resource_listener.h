/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_BROADCAST_RESOURCE_LISTENER_H_
#define CALL_ADAPTATION_BROADCAST_RESOURCE_LISTENER_H_

#include <vector>

#include "api/adaptation/resource.h"
#include "api/scoped_refptr.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

// Responsible for forwarding 1 resource usage measurement to N listeners by
// creating N "adapter" resources.
//
// Example:
// If we have ResourceA, ResourceListenerX and ResourceListenerY we can create a
// BroadcastResourceListener that listens to ResourceA, use CreateAdapter() to
// spawn adapter resources ResourceX and ResourceY and let ResourceListenerX
// listen to ResourceX and ResourceListenerY listen to ResourceY. When ResourceA
// makes a measurement it will be echoed by both ResourceX and ResourceY.
class BroadcastResourceListener : public ResourceListener {
 public:
  explicit BroadcastResourceListener(
      rtc::scoped_refptr<Resource> source_resource);
  ~BroadcastResourceListener() override;

  // Creates a Resource that redirects any resource usage measurements that
  // BroadcastResourceListener receives to its listener. The adapter has to be
  // unregistered before BroadcastResourceListener is destroyed, which happens
  // when the adapter's listener is set to null.
  rtc::scoped_refptr<Resource> CreateAdapter();

  // ResourceListener implementation.
  void OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource> resource,
                                    ResourceUsageState usage_state) override;

  std::vector<rtc::scoped_refptr<Resource>> GetAdaptersForTesting();

 private:
  class AdapterResource;
  friend class AdapterResource;

  void RegisterAdapter(rtc::scoped_refptr<AdapterResource> adapter);
  void UnregisterAdapter(rtc::scoped_refptr<AdapterResource> adapter);

  const rtc::scoped_refptr<Resource> source_resource_;
  rtc::CriticalSection lock_;
  // The AdapterResource unregisters itself prior to destruction, guaranteeing
  // that these pointers are safe to use.
  std::vector<rtc::scoped_refptr<AdapterResource>> adapters_
      RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_BROADCAST_RESOURCE_LISTENER_H_
