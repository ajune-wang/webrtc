#ifndef NET_DCSCTP_PACKET_CHUNK_VALIDATORS_H_
#define NET_DCSCTP_PACKET_CHUNK_VALIDATORS_H_

#include "net/dcsctp/packet/chunk/sack_chunk.h"

namespace dcsctp {
// Validates and cleans SCTP chunks.
class ChunkValidators {
 public:
  // Given a SackChunk, it will return a cleaned and validated variant of it.
  // RFC4960 doesn't say anything about validity of SACKs or if the Gap ACK
  // blocks must be sorted, and non-overlapping. While they always are in
  // well-behaving implementations, this can't be relied on.
  static SackChunk Clean(SackChunk&& sack);
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_CHUNK_VALIDATORS_H_
