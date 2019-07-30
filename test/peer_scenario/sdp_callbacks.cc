/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/peer_scenario/sdp_callbacks.h"

#include <utility>

namespace webrtc {
namespace test {
SdpSetObserversInterface* SdpSetObserver(
    std::function<void(RTCError)> callback) {
  class SdpSetObserver : public SdpSetObserversInterface {
   public:
    explicit SdpSetObserver(std::function<void(RTCError)> callback)
        : callback_(std::move(callback)) {}
    void OnSuccess() override { callback_(RTCError::OK()); }
    void OnFailure(RTCError error) override { callback_(std::move(error)); }
    void OnSetRemoteDescriptionComplete(RTCError error) override {
      callback_(std::move(error));
    }
    std::function<void(RTCError)> callback_;
  };
  return new SdpSetObserver(std::move(callback));
}

SdpSetObserversInterface* SdpSetObserver(std::function<void()> callback) {
  return SdpSetObserver([callback](RTCError error) {
    RTC_CHECK(error.ok()) << error.message();
    callback();
  });
}

CreateSessionDescriptionObserver* SdpCreateObserver(
    std::function<void(RTCErrorOr<SessionDescriptionInterface*>)> callback) {
  class SdpCreateObserver
      : public rtc::RefCountedObject<CreateSessionDescriptionObserver> {
   public:
    explicit SdpCreateObserver(decltype(callback) callback)
        : callback_(std::move(callback)) {}
    void OnSuccess(SessionDescriptionInterface* desc) override {
      callback_(desc);
    }
    void OnFailure(RTCError error) override { callback_(std::move(error)); }
    decltype(callback) callback_;
  };
  return new SdpCreateObserver(std::move(callback));
}

CreateSessionDescriptionObserver* SdpCreateObserver(
    std::function<void(SessionDescriptionInterface*)> callback) {
  std::function<void(RTCErrorOr<SessionDescriptionInterface*>)> wrapper(
      [callback](RTCErrorOr<SessionDescriptionInterface*> error) {
        callback(error.value());
      });
  return SdpCreateObserver(std::move(wrapper));
}

}  // namespace test
}  // namespace webrtc
