#ifndef NET_DCSCTP_PACKET_CHUNK_COOKIE_ACK_CHUNK_H_
#define NET_DCSCTP_PACKET_CHUNK_COOKIE_ACK_CHUNK_H_
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/tlv_trait.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.12
struct CookieAckChunkConfig : ChunkConfig {
  static constexpr int kType = 11;
  static constexpr size_t kHeaderSize = 4;
  static constexpr size_t kVariableLengthAlignment = 0;
};

class CookieAckChunk : public Chunk, public TLVTrait<CookieAckChunkConfig> {
 public:
  static constexpr int kType = CookieAckChunkConfig::kType;

  explicit CookieAckChunk() {}

  static absl::optional<CookieAckChunk> Parse(
      rtc::ArrayView<const uint8_t> data);

  void SerializeTo(std::vector<uint8_t>& out) const override;
  std::string ToString() const override;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_CHUNK_COOKIE_ACK_CHUNK_H_
