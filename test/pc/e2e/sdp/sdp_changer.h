/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_SDP_SDP_CHANGER_H_
#define TEST_PC_E2E_SDP_SDP_CHANGER_H_

#include <memory>
#include <string>

#include "api/jsep.h"

namespace webrtc {
namespace test {

class SdpChanger {
 public:
  explicit SdpChanger(
      std::unique_ptr<SessionDescriptionInterface> session_description);
  ~SdpChanger();

  // Force use of video codec with name |codec_name| on track with stream
  // |stream_label| by putting this codec on the first place in codecs list.
  // Specified codec should exist in codecs list, otherwise invocation will
  // fail.
  void ForceVideoCodec(const std::string& stream_label, std::string codec_name);

  // Returns changed session description. Any future invocations of any method
  // on this object are forbidden.
  std::unique_ptr<SessionDescriptionInterface> ReleaseSessionDescription();

 private:
  std::unique_ptr<SessionDescriptionInterface> session_description_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_SDP_SDP_CHANGER_H_
