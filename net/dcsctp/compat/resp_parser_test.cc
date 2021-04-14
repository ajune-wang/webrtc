#include "net/dcsctp/compat/resp_parser.h"

#include "rtc_base/gunit.h"
#include "test/gmock.h"
namespace dcsctp {
namespace compat {
namespace {
using ::testing::SizeIs;

TEST(RespParserTest, ParseHelloWorld) {
  absl::string_view data = "*3\r\n$3\r\nset\r\n$5\r\nhello\r\n$5\r\nworld\r\n";

  Stream stream(data);
  absl::optional<ArrayRespType::ItemType> parsed = ParseResp(stream);
  ASSERT_TRUE(parsed.has_value());

  ArrayRespType& arr = absl::get<ArrayRespType>(*parsed);
  EXPECT_THAT(arr.items(), SizeIs(3));

  BulkStringRespType item0 = absl::get<BulkStringRespType>(arr.items()[0]);
  EXPECT_EQ(item0.str(), "set");

  BulkStringRespType item1 = absl::get<BulkStringRespType>(arr.items()[1]);
  EXPECT_EQ(item1.str(), "hello");

  BulkStringRespType item2 = absl::get<BulkStringRespType>(arr.items()[2]);
  EXPECT_EQ(item2.str(), "world");

  RTC_LOG(LS_INFO) << ToString(*parsed);
}

TEST(RespParserTest, MissingArrayElement) {
  absl::string_view data = "*3\r\n$3\r\nset\r\n$5\r\nhello\r\n";

  Stream stream(data);
  ASSERT_FALSE(ParseResp(stream).has_value());
}

TEST(RespParserTest, TooSmallString) {
  absl::string_view data = "*1\r\n$5\r\nset";

  Stream stream(data);
  ASSERT_FALSE(ParseResp(stream).has_value());
}
}  // namespace
}  // namespace compat
}  // namespace dcsctp
