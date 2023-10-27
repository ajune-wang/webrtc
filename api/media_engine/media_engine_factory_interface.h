/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_MEDIA_ENGINE_MEDIA_ENGINE_FACTORY_INTERFACE_H_
#define API_MEDIA_ENGINE_MEDIA_ENGINE_FACTORY_INTERFACE_H_

#include <memory>

#include "api/call/call_factory_interface.h"

namespace cricket {
// These classes are not part of the API, and are treated as opaque pointers.
class MediaEngineInterface;
}  // namespace cricket

namespace webrtc {

struct PeerConnectionFactoryDependencies;

// This interface exists to allow webrtc to be optionally built without media
// support (i.e., if only being used for data channels). PeerConnectionFactory
// is constructed with a MediaEngineFactoryInterface, which may or may not be
// null.
// TODO(bugs.webrtc.org/15574): Delete CallFactoryInterface when call_factory
// is removed from PeerConnectionFactoryDependencies
class MediaEngineFactoryInterface : public CallFactoryInterface {
 public:
  virtual ~MediaEngineFactoryInterface() = default;

 private:
  // Usage of this interface is webrtc implementation details.
  friend class ConnectionContext;
  friend class MediaEngineFactoryForTest;
  std::unique_ptr<Call> CreateCall(const CallConfig& config) override = 0;
  virtual std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryDependencies& dependencies) = 0;
};

}  // namespace webrtc

#endif  // API_MEDIA_ENGINE_MEDIA_ENGINE_FACTORY_INTERFACE_H_
