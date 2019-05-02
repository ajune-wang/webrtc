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

#include <map>
#include <string>

#include "api/jsep.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Force use of video codec with name |codec_name| on all tracks by removing
// all known codecs (VP8, VP9, H264) from the codecs list and keeping only
// specified one and putting it on the first place.
//
// Specified codec should exist in codecs list, otherwise invocation will
// fail.
void ForceVideoCodec(
    SessionDescriptionInterface* session_description,
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params);

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_SDP_SDP_CHANGER_H_
