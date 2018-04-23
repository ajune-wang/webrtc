/*
 * fake_opus_decode_from_file.cc
 *
 *  Created on: Apr 18, 2018
 *      Author: minyue
 */

#include "modules/audio_coding/neteq/tools/fake_opus_decode_from_file.h"

namespace webrtc {
namespace test {

bool FakeOpusDecodeFromFile::IsCngPacket(size_t payload_size_bytes) {
  // Audio type becomes comfort noise if |encoded_byte| is 1 and keeps
  // to be so if the following |encoded_byte| are 0 or 1.
  if (payload_size_bytes == 0 && dtx_mode_) {
    return true;  // Comfort noise.
  } else if (payload_size_bytes == 1 || payload_size_bytes == 2) {
    // TODO(henrik.lundin): There is a slight risk that a 2-byte payload is in
    // fact a 1-byte TOC with a 1-byte payload. That will be erroneously
    // interpreted as comfort noise output, but such a payload is probably
    // faulty anyway.
    dtx_mode_ = true;
    return true;  // Comfort noise.
  } else {
    dtx_mode_ = false;
    return false;  // Speech.
  }
}

}  // namespace test
}  // namespace webrtc
