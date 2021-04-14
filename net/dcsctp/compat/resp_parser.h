#ifndef NET_DCSCTP_COMPAT_RESP_PARSER_H_
#define NET_DCSCTP_COMPAT_RESP_PARSER_H_

#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {
namespace compat {

class RespType {
 public:
};

class Stream {
 public:
  explicit Stream(absl::string_view data)
      : data_(rtc::ArrayView<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size())) {}
  explicit Stream(rtc::ArrayView<const uint8_t> data) : data_(data) {}

  absl::optional<absl::string_view> GetLine();
  std::vector<uint8_t> Read(size_t count);
  uint8_t Read();
  size_t remaining() const { return data_.size() - offset_; }
  size_t offset() const { return offset_; }

 private:
  size_t offset_ = 0;
  rtc::ArrayView<const uint8_t> data_;
};

class SimpleStringRespType : RespType {
 public:
  static constexpr uint8_t kCommandType = '+';

  explicit SimpleStringRespType(absl::string_view value) : value_(value) {}

  static absl::optional<SimpleStringRespType> Parse(Stream& stream);

  absl::string_view value() const { return value_; }
  void ToString(rtc::StringBuilder& sb) const;

 private:
  std::string value_;
};

class ErrorRespType : RespType {
 public:
  static constexpr uint8_t kCommandType = '-';

  explicit ErrorRespType(absl::string_view value) : value_(value) {}

  static absl::optional<ErrorRespType> Parse(Stream& stream);

  absl::string_view value() const { return value_; }
  void ToString(rtc::StringBuilder& sb) const;

 private:
  std::string value_;
};

class IntegerRespType : RespType {
 public:
  static constexpr uint8_t kCommandType = ':';

  explicit IntegerRespType(int64_t value) : value_(value) {}

  static absl::optional<IntegerRespType> Parse(Stream& stream);

  int64_t value() const { return value_; }
  void ToString(rtc::StringBuilder& sb) const;

 private:
  int64_t value_;
};

class BulkStringRespType : RespType {
 public:
  static constexpr uint8_t kCommandType = '$';

  explicit BulkStringRespType(std::vector<uint8_t> data)
      : data_(std::move(data)), is_null_(false) {}

  BulkStringRespType() : data_({}), is_null_(true) {}

  static absl::optional<BulkStringRespType> Parse(Stream& stream);

  const std::vector<uint8_t>& data() const { return data_; }
  absl::string_view str() const {
    return absl::string_view(reinterpret_cast<const char*>(data_.data()),
                             data_.size());
  }
  bool is_null() const { return is_null_; }
  void ToString(rtc::StringBuilder& sb) const;

 private:
  std::vector<uint8_t> data_;
  bool is_null_;
};

class ArrayRespType : RespType {
 public:
  static constexpr uint8_t kCommandType = '*';
  using ItemType = absl::variant<SimpleStringRespType,
                                 ErrorRespType,
                                 IntegerRespType,
                                 BulkStringRespType,
                                 ArrayRespType>;

  explicit ArrayRespType(std::vector<ItemType> items)
      : items_(std::move(items)), is_null_(false) {}
  ArrayRespType() : is_null_(true) {}

  static absl::optional<ArrayRespType> Parse(Stream& stream);

  const std::vector<ItemType>& items() const { return items_; }
  bool is_null() const { return is_null_; }
  void ToString(rtc::StringBuilder& sb) const;

 private:
  std::vector<ItemType> items_;
  bool is_null_;
};

absl::optional<ArrayRespType::ItemType> ParseResp(Stream& stream);

std::string ToString(const ArrayRespType::ItemType& value);
void AddToString(const ArrayRespType::ItemType& value, rtc::StringBuilder& sb);

class RespCommandBuffer {
 public:
  void Add(rtc::ArrayView<const uint8_t> data);

  absl::optional<ArrayRespType::ItemType> GetItem();

 private:
  std::vector<uint8_t> buffered_data_;
};
}  // namespace compat
}  // namespace dcsctp

#endif  // NET_DCSCTP_COMPAT_RESP_PARSER_H_
