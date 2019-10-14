/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_NETEQ_CONTROLLER_FACTORY_H_
#define API_NETEQ_NETEQ_CONTROLLER_FACTORY_H_

#include <memory>

#include "api/neteq/neteq_controller.h"

namespace webrtc {

class NetEqControllerFactory {
 public:
  NetEqControllerFactory(const NetEqControllerFactory&) = delete;
  NetEqControllerFactory& operator=(const NetEqControllerFactory&) = delete;
  virtual ~NetEqControllerFactory();

  // Creates a new NetEqController object, with parameters set in |config|.
  // The |config| object will only have to be valid for the duration of the call
  // to this method.
  virtual std::unique_ptr<NetEqController> CreateNetEqController(
      const NetEqController::Config& config) const = 0;

 protected:
  NetEqControllerFactory();
};

}  // namespace webrtc
#endif  // API_NETEQ_NETEQ_CONTROLLER_FACTORY_H_
