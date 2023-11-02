/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_ENABLE_FAKE_MEDIA_H_
#define PC_TEST_ENABLE_FAKE_MEDIA_H_

#include <memory>

#include "absl/base/nullability.h"
#include "api/peer_connection_interface.h"
#include "media/base/fake_media_engine.h"

namespace webrtc {

void EnableFakeMedia(
    PeerConnectionFactoryDependencies& deps,
    absl::Nonnull<std::unique_ptr<cricket::FakeMediaEngine>> fake);

inline void EnableFakeMedia(PeerConnectionFactoryDependencies& deps) {
  EnableFakeMedia(deps, std::make_unique<cricket::FakeMediaEngine>());
}

}  // namespace webrtc

#endif  //  PC_TEST_ENABLE_FAKE_MEDIA_H_
