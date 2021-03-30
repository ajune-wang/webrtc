#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::SizeIs;

TEST(CookieAckChunkTest, FromCapture) {
  /*
  COOKIE_ACK chunk
      Chunk type: COOKIE_ACK (11)
      Chunk flags: 0x00
      Chunk length: 4
  */

  uint8_t data[] = {0x0b, 0x00, 0x00, 0x04};

  EXPECT_TRUE(CookieAckChunk::Parse(data).has_value());
}

TEST(CookieAckChunkTest, SerializeAndDeserialize) {
  CookieAckChunk chunk;

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(CookieAckChunk deserialized,
                              CookieAckChunk::Parse(serialized));
  EXPECT_EQ(deserialized.ToString(), "COOKIE-ACK");
}

}  // namespace
}  // namespace dcsctp
