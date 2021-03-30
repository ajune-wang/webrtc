#include "net/dcsctp/packet/chunk/shutdown_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"

namespace dcsctp {
namespace {
TEST(ShutdownChunkTest, FromCapture) {
  /*
    SHUTDOWN chunk (Cumulative TSN ack: 101831101)
      Chunk type: SHUTDOWN (7)
      Chunk flags: 0x00
      Chunk length: 8
      Cumulative TSN Ack: 101831101
      */

  uint8_t data[] = {0x07, 0x00, 0x00, 0x08, 0x06, 0x11, 0xd1, 0xbd};

  ASSERT_HAS_VALUE_AND_ASSIGN(ShutdownChunk chunk, ShutdownChunk::Parse(data));
  EXPECT_EQ(chunk.cumulative_tsn_ack(), 101831101u);
}

TEST(ShutdownChunkTest, SerializeAndDeserialize) {
  ShutdownChunk chunk(12345678);

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(ShutdownChunk deserialized,
                              ShutdownChunk::Parse(serialized));

  EXPECT_EQ(deserialized.cumulative_tsn_ack(), 12345678u);
}

}  // namespace
}  // namespace dcsctp
