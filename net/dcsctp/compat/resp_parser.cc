#include "net/dcsctp/compat/resp_parser.h"

#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "rtc_base/logging.h"

namespace dcsctp {
namespace compat {

absl::optional<absl::string_view> Stream::GetLine() {
  absl::string_view buffer(
      reinterpret_cast<const char*>(data_.data() + offset_), remaining());

  size_t pos = buffer.find("\r\n");
  if (pos == absl::string_view::npos) {
    RTC_DLOG(LS_VERBOSE) << "Failed to find string at position " << offset_;
    return absl::nullopt;
  }

  absl::string_view result = buffer.substr(0, pos);

  // Skipping \r\n
  offset_ += result.size() + 2;

  return result;
}

std::vector<uint8_t> Stream::Read(size_t count) {
  RTC_DCHECK(count <= remaining());

  count = std::min(count, remaining());
  std::vector<uint8_t> result(count);
  memcpy(result.data(), data_.data() + offset_, count);
  offset_ += count;
  return result;
}

uint8_t Stream::Read() {
  RTC_DCHECK(remaining() > 0);
  uint8_t val = 0;
  if (remaining() > 0) {
    val = data_[offset_];
    ++offset_;
  }
  return val;
}

absl::optional<SimpleStringRespType> SimpleStringRespType::Parse(
    Stream& stream) {
  absl::optional<absl::string_view> line = stream.GetLine();
  if (!line.has_value()) {
    RTC_DLOG(LS_VERBOSE) << "Failed to read SimpleString - not enough data?";
    return absl::nullopt;
  }
  return SimpleStringRespType(*line);
}

void SimpleStringRespType::ToString(rtc::StringBuilder& sb) const {
  sb << "'" << value_ << "'";
}

absl::optional<ErrorRespType> ErrorRespType::Parse(Stream& stream) {
  absl::optional<absl::string_view> line = stream.GetLine();
  if (!line.has_value()) {
    RTC_DLOG(LS_VERBOSE) << "Failed to read Error - not enough data?";
    return absl::nullopt;
  }
  return ErrorRespType(*line);
}

void ErrorRespType::ToString(rtc::StringBuilder& sb) const {
  sb << "error:\"" << value_ << "\"";
}

absl::optional<IntegerRespType> IntegerRespType::Parse(Stream& stream) {
  absl::optional<absl::string_view> line = stream.GetLine();
  if (!line.has_value()) {
    RTC_DLOG(LS_VERBOSE) << "Failed to read Integer - not enough data?";
    return absl::nullopt;
  }

  int64_t value;
  if (!absl::SimpleAtoi(*line, &value)) {
    RTC_LOG(LS_WARNING) << "Failed to parse Integer - invalid data - at offset "
                        << stream.offset();
    return absl::nullopt;
  }

  return IntegerRespType(value);
}

void IntegerRespType::ToString(rtc::StringBuilder& sb) const {
  sb << value_;
}

absl::optional<BulkStringRespType> BulkStringRespType::Parse(Stream& stream) {
  absl::optional<absl::string_view> size_line = stream.GetLine();
  if (!size_line.has_value()) {
    RTC_DLOG(LS_VERBOSE)
        << "Failed to read BulkString length - not enough data?";
    return absl::nullopt;
  }

  int32_t size;
  if (!absl::SimpleAtoi(*size_line, &size)) {
    RTC_LOG(LS_WARNING)
        << "Failed to parse BulkString length - invalid data - at offset "
        << stream.offset();
    return absl::nullopt;
  }

  if (size < 0) {
    return BulkStringRespType();
  } else if (size > 512 * 1024 * 1024) {
    RTC_LOG(LS_WARNING)
        << "Failed to parse BulkString length - too large - at offset "
        << stream.offset();
    return absl::nullopt;
  }

  if (stream.remaining() < static_cast<size_t>(size + 2)) {
    RTC_LOG(LS_WARNING) << "Failed to parse BulkString length - not enough "
                           "remaining - at offset "
                        << stream.offset();
    return absl::nullopt;
  }

  std::vector<uint8_t> data = stream.Read(size);
  // Skip \r\n
  stream.GetLine();

  return BulkStringRespType(std::move(data));
}

void BulkStringRespType::ToString(rtc::StringBuilder& sb) const {
  sb << "\"" << str() << "\"";
}

absl::optional<ArrayRespType> ArrayRespType::Parse(Stream& stream) {
  absl::optional<absl::string_view> size_line = stream.GetLine();
  if (!size_line.has_value()) {
    RTC_DLOG(LS_VERBOSE) << "Failed to read Array length - not enough data?";
    return absl::nullopt;
  }

  int32_t size;
  if (!absl::SimpleAtoi(*size_line, &size)) {
    RTC_LOG(LS_WARNING)
        << "Failed to parse Array length - invalid data - at offset "
        << stream.offset();
    return absl::nullopt;
  }

  if (size < 0) {
    return ArrayRespType();
  }

  std::vector<ArrayRespType::ItemType> items;
  for (int i = 0; i < size; ++i) {
    absl::optional<ItemType> item = ParseResp(stream);
    if (!item.has_value()) {
      return absl::nullopt;
    }
    items.emplace_back(*std::move(item));
  }

  return ArrayRespType(std::move(items));
}

void ArrayRespType::ToString(rtc::StringBuilder& sb) const {
  sb << "[";
  for (size_t i = 0; i < items_.size(); ++i) {
    if (i > 0) {
      sb << ",";
    }
    AddToString(items_[i], sb);
  }
  sb << "]";
}

absl::optional<ArrayRespType::ItemType> ParseResp(Stream& stream) {
  if (stream.remaining() == 0) {
    RTC_LOG(LS_WARNING)
        << "Failed to read Array item command - not enough data?";
    return absl::nullopt;
  }

  char type = stream.Read();
  switch (type) {
    case SimpleStringRespType::kCommandType:
      return SimpleStringRespType::Parse(stream);

    case ErrorRespType::kCommandType:
      return ErrorRespType::Parse(stream);
    case BulkStringRespType::kCommandType:
      return BulkStringRespType::Parse(stream);

    case ArrayRespType::kCommandType:
      return ArrayRespType::Parse(stream);
    default: {
      RTC_LOG(LS_WARNING) << "Failed to parse Array item type - invalid type="
                          << static_cast<int>(type)
                          << " at offset=" << stream.offset();

      return absl::nullopt;
    }
  }
}

struct PrintVisitor {
  rtc::StringBuilder& sb;

  template <typename T>
  void operator()(const T& value) const {
    value.ToString(sb);
  }
};

std::string ToString(const ArrayRespType::ItemType& value) {
  rtc::StringBuilder sb;
  AddToString(value, sb);
  return sb.Release();
}

void AddToString(const ArrayRespType::ItemType& value, rtc::StringBuilder& sb) {
  absl::visit(PrintVisitor{sb}, value);
}

void RespCommandBuffer::Add(rtc::ArrayView<const uint8_t> data) {
  buffered_data_.insert(buffered_data_.end(), data.begin(), data.end());
}

absl::optional<ArrayRespType::ItemType> RespCommandBuffer::GetItem() {
  Stream stream(buffered_data_);
  return absl::nullopt;
}

}  // namespace compat
}  // namespace dcsctp
