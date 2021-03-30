#include "net/dcsctp/packet/chunk/shutdown_ack_chunk.h"

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.9

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |   Type = 8    |Chunk  Flags   |      Length = 4               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int ShutdownAckChunk::kType;

absl::optional<ShutdownAckChunk> ShutdownAckChunk::Parse(
    rtc::ArrayView<const uint8_t> data) {
  if (!ParseTLV(data).has_value()) {
    return absl::nullopt;
  }
  return ShutdownAckChunk();
}

void ShutdownAckChunk::SerializeTo(std::vector<uint8_t>& out) const {
  AllocateTLV(out);
}

std::string ShutdownAckChunk::ToString() const {
  return "SHUTDOWN-ACK";
}

}  // namespace dcsctp
