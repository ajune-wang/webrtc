/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtp_dump_replayer.h"

namespace webrtc {
namespace {

// Configuration is directly coded into the file to prevent disk access during
// fuzzing which requires the ability to fork at high speed.
const char kConfiguration[] =
    "["
    "   {"
    "      \"decoders\" : ["
    "         {"
    "            \"codec_params\" : ["
    "               {"
    "                  \"level-asymmetry-allowed\" : \"1\""
    "               },"
    "               {"
    "                  \"packetization-mode\" : \"1\""
    "               },"
    "               {"
    "                  \"profile-level-id\" : \"42001f\""
    "               }"
    "            ],"
    "            \"payload_name\" : \"H264\","
    "            \"payload_type\" : 100"
    "         },"
    "         {"
    "            \"codec_params\" : ["
    "               {"
    "                  \"level-asymmetry-allowed\" : \"1\""
    "               },"
    "               {"
    "                  \"packetization-mode\" : \"1\""
    "               },"
    "               {"
    "                  \"profile-level-id\" : \"42e01f\""
    "               }"
    "            ],"
    "            \"payload_name\" : \"H264\","
    "            \"payload_type\" : 102"
    "         }"
    "      ],"
    "      \"render_delay_ms\" : 10,"
    "      \"rtp\" : {"
    "         \"extensions\" : [],"
    "         \"local_ssrc\" : 1,"
    "         \"nack\" : {"
    "            \"rtp_history_ms\" : 1000"
    "         },"
    "         \"red_payload_type\" : 125,"
    "         \"remb\" : true,"
    "         \"remote_ssrc\" : 1989790381,"
    "         \"rtcp_mode\" : \"RtcpMode::kReducedSize\","
    "         \"rtx_payload_types\" : ["
    "            {"
    "               \"101\" : 100"
    "            },"
    "            {"
    "               \"122\" : 125"
    "            },"
    "            {"
    "               \"123\" : 127"
    "            }"
    "         ],"
    "         \"rtx_ssrc\" : 1406083315,"
    "         \"transport_cc\" : true,"
    "         \"ulpfec_payload_type\" : 124"
    "      },"
    "      \"target_delay_ms\" : 0"
    "   }"
    "]";

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  rtc::ArrayView<const uint8_t> rtp_dump(data, size);
  auto stream_state =
      test::RtpDumpReplayer::StreamState::FromString(kConfiguration);
  if (stream_state != nullptr) {
    test::RtpDumpReplayer::Replay(std::move(stream_state), rtp_dump);
  }
}

}  // namespace webrtc
