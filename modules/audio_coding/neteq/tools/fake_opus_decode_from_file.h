/*
 * fake_opus_decode_from_file.h
 *
 *  Created on: Apr 18, 2018
 *      Author: minyue
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_FAKE_OPUS_DECODE_FROM_FILE_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_FAKE_OPUS_DECODE_FROM_FILE_H_

#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"

namespace webrtc {
namespace test {

class FakeOpusDecodeFromFile : public FakeDecodeFromFile {
 public:
  FakeOpusDecodeFromFile(std::unique_ptr<InputAudioFile> input,
                         int sample_rate_hz,
                         bool stereo)
      : FakeDecodeFromFile(std::move(input), sample_rate_hz, stereo) {}

 private:
  bool IsCngPacket(size_t payload_size_bytes) override;
  bool dtx_mode_ = false;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_FAKE_OPUS_DECODE_FROM_FILE_H_
