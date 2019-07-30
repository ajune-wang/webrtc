/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PEER_SCENARIO_SDP_CALLBACKS_H_
#define TEST_PEER_SCENARIO_SDP_CALLBACKS_H_

#include "api/peer_connection_interface.h"

namespace webrtc {
namespace test {

class SdpSetObserversInterface
    : public rtc::RefCountedObject<SetSessionDescriptionObserver>,
      public rtc::RefCountedObject<SetRemoteDescriptionObserverInterface> {};

SdpSetObserversInterface* SdpSetObserver(
    std::function<void(RTCError)> callback);

SdpSetObserversInterface* SdpSetObserver(std::function<void()> callback);

CreateSessionDescriptionObserver* SdpCreateObserver(
    std::function<void(RTCErrorOr<SessionDescriptionInterface*>)> callback);

CreateSessionDescriptionObserver* SdpCreateObserver(
    std::function<void(SessionDescriptionInterface*)> callback);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PEER_SCENARIO_SDP_CALLBACKS_H_
