#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.12

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |   Type = 11   |Chunk  Flags   |     Length = 4                |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int CookieAckChunk::kType;

absl::optional<CookieAckChunk> CookieAckChunk::Parse(
    rtc::ArrayView<const uint8_t> data) {
  if (!ParseTLV(data).has_value()) {
    return absl::nullopt;
  }
  return CookieAckChunk();
}

void CookieAckChunk::SerializeTo(std::vector<uint8_t>& out) const {
  AllocateTLV(out);
}

std::string CookieAckChunk::ToString() const {
  return "COOKIE-ACK";
}

}  // namespace dcsctp
