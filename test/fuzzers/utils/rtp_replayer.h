/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FUZZERS_UTILS_RTP_REPLAYER_H_
#define TEST_FUZZERS_UTILS_RTP_REPLAYER_H_

#include <stdio.h>

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/engine/internal_decoder_factory.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_config_utils.h"
#include "test/call_test.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/rtp_file_reader.h"
#include "test/run_loop.h"
#include "test/run_test.h"
#include "test/test_video_capturer.h"
#include "test/testsupport/frame_writer.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace test {

class RtpReplayer final {
 public:
  static void Replay(const std::string& replay_config,
                     const uint8_t* rtp_dump_data,
                     size_t rtp_dump_size);

 private:
  struct StreamState {
    test::NullTransport transport;
    std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
    std::vector<VideoReceiveStream*> receive_streams;
    std::unique_ptr<VideoDecoderFactory> decoder_factory;
  };

  static std::unique_ptr<StreamState> ConfigureForFuzzer(
      const std::string& replay_config,
      Call* call);
  static std::unique_ptr<test::RtpFileReader> CreateRtpReader(
      const uint8_t* rtp_dump_data,
      size_t rtp_dump_size);
  static void ReplayPackets(Call* call, test::RtpFileReader* rtp_reader);
};  // class RtpReplayer

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FUZZERS_UTILS_RTP_REPLAYER_H_
