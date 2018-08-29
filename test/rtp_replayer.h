/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_RTP_REPLAYER_H_
#define TEST_RTP_REPLAYER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "test/rtp_file_reader.h"

namespace webrtc {
namespace test {

// The RtpReplayer is designed to be used in fuzzing and testing scenarios where
// you need to configure playback of a previously captured RtpDump or Pcap. This
// class lets you specify a customizable stream state that can be configured
// from a JSON file or manually and the respective packets to replay. The
// simplest usage is just:
// RtpReplayer::Replay(StreamState::Load(config_path), rtp_dump_path);
class RtpReplayer final {
 public:
  // Holds all the shared memory structures required for a recieve stream. This
  // structure is used to prevent members being deallocated before the replay
  // has been finished.
  struct StreamState final {
    StreamState();
    ~StreamState();

    // Loads multiple configurations from the provided configuration file.
    static std::unique_ptr<StreamState> Load(const std::string& config_path);
    // Loads the configuration directly from a string instead of a file.
    static std::unique_ptr<StreamState> FromString(
        const std::string& config_string);

    std::unique_ptr<Call> call;
    test::NullTransport transport;
    std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
    std::vector<VideoReceiveStream*> receive_streams;
  };

  // Replay an rtp dump with a provided stream state.
  static void Replay(std::unique_ptr<StreamState> stream_state,
                     const std::string& rtp_dump_path);

 private:
  // Creates an RTPReader to parse the packet dump.
  static std::unique_ptr<test::RtpFileReader> CreateRtpReader(
      const std::string& rtp_dump_path);
  // Replays all the packets found in the packet dump.
  static void ReplayPackets(Call* call, test::RtpFileReader* rtp_reader);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RTP_REPLAYER_H_
