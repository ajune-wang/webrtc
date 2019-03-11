/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <string>

#include "test/fuzzers/utils/rtp_replayer.h"

namespace webrtc {
namespace {

constexpr char vp9_replay_config[] =
    "[{"
    "      \"decoders\" : ["
    "         {"
    "            \"codec_params\" : [],"
    "            \"payload_name\" : \"VP9\","
    "            \"payload_type\" : 124"
    "         }"
    "      ],"
    "      \"render_delay_ms\" : 10,"
    "      \"rtp\" : {"
    "         \"extensions\" : [],"
    "         \"local_ssrc\" : 7331,"
    "         \"nack\" : {"
    "            \"rtp_history_ms\" : 1000"
    "         },"
    "         \"red_payload_type\" : -1,"
    "         \"remb\" : true,"
    "         \"remote_ssrc\" : 1337,"
    "         \"rtcp_mode\" : \"RtcpMode::kCompound\","
    "         \"rtx_payload_types\" : [],"
    "         \"rtx_ssrc\" : 100,"
    "         \"transport_cc\" : true,"
    "         \"ulpfec_payload_type\" : -1"
    "      },"
    "      \"target_delay_ms\" : 0"
    "}]";

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  test::RtpReplayer::Replay(vp9_replay_config, data, size);
}

}  // namespace webrtc
