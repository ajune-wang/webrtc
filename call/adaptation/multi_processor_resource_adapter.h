/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_MULTI_PROCESSOR_RESOURCE_ADAPTER_H_
#define CALL_ADAPTATION_MULTI_PROCESSOR_RESOURCE_ADAPTER_H_

#include <vector>

#include "api/adaptation/resource.h"
#include "api/scoped_refptr.h"

namespace webrtc {

class MultiProcessorResourceAdapter : public ResourceListener {
 public:
  ~MultiProcessorResourceAdapter() override;

  rtc::scoped_refptr<Resource> CreateAdapter();

  // ResourceListener implementation.
  void OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource> resource,
                                    ResourceUsageState usage_state) override;

 private:
  class AdapterResource;
  friend class AdapterResource;

  // The AdapterResource unregisters itself on destruction, guaranteeing that
  // these pointers are always safe to use.
  std::vector<AdapterResource*> adapters_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_MULTI_PROCESSOR_RESOURCE_ADAPTER_H_
