#include "net/dcsctp/packet/chunk/shutdown_complete_chunk.h"

#include <stdint.h>

#include <vector>

#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::SizeIs;

TEST(ShutdownCompleteChunkTest, FromCapture) {
  /*
    SHUTDOWN_COMPLETE chunk
      Chunk type: SHUTDOWN_COMPLETE (14)
      Chunk flags: 0x00
      Chunk length: 4
  */

  uint8_t data[] = {0x0e, 0x00, 0x00, 0x04};

  EXPECT_TRUE(ShutdownCompleteChunk::Parse(data).has_value());
}

TEST(ShutdownCompleteChunkTest, SerializeAndDeserialize) {
  ShutdownCompleteChunk chunk(/*tag_reflected=*/false);

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  EXPECT_TRUE(ShutdownCompleteChunk::Parse(serialized).has_value());
}

}  // namespace
}  // namespace dcsctp
