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
#include <vector>

#include "absl/strings/string_view.h"
#include "api/rtp_parameters.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Creates list of capabilities, which can be set on RtpTransceiverInterface via
// RtpTransceiverInterface::SetCodecPreferences(...) to force use of codec with
// specified |codec_name| and parameters, which contains all of
// |codec_required_params|. Also it will include corresponding FEC codecs if
// flags |ulpfec| or |flexfec| set to true.
// All codecs will added only if they exists in the list of |supported_codecs|.
std::vector<RtpCodecCapability> ForceVideoCodec(
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params,
    bool ulpfec,
    bool flexfec,
    std::vector<RtpCodecCapability> supported_codecs);

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_SDP_SDP_CHANGER_H_
