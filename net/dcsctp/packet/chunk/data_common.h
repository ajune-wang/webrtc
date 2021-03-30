#ifndef NET_DCSCTP_PACKET_CHUNK_DATA_COMMON_H_
#define NET_DCSCTP_PACKET_CHUNK_DATA_COMMON_H_
#include <stdint.h>

#include <utility>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/data.h"

namespace dcsctp {

// Data chunk options.
// See https://tools.ietf.org/html/rfc4960#section-3.3.1
struct DataChunkOptions {
  bool is_end = false;
  bool is_beginning = false;
  bool is_unordered = false;
  bool immediate_ack = false;  // From RFC7053
};

// Base class for DataChunk and IDataChunk
class AnyDataChunk : public Chunk {
 public:
  uint32_t tsn() const { return tsn_; }

  DataChunkOptions options() const {
    return DataChunkOptions{
        .is_end = data_.is_end,
        .is_beginning = data_.is_beginning,
        .is_unordered = data_.is_unordered,
        .immediate_ack = immediate_ack_,
    };
  }
  uint16_t stream_id() const { return data_.stream_id; }
  uint16_t ssn() const { return data_.ssn; }
  uint32_t message_id() const { return data_.message_id; }
  uint32_t fsn() const { return data_.fsn; }
  uint32_t ppid() const { return data_.ppid; }
  rtc::ArrayView<const uint8_t> payload() const { return data_.payload; }

  // Extracts the Data from the chunk, as a destructive action.
  Data extract() && { return std::move(data_); }

  AnyDataChunk(uint32_t tsn,
               uint16_t stream_id,
               uint16_t ssn,
               uint32_t message_id,
               uint32_t fsn,
               uint32_t ppid,
               std::vector<uint8_t> payload,
               const DataChunkOptions& options)
      : tsn_(tsn),
        data_(stream_id,
              ssn,
              message_id,
              fsn,
              ppid,
              std::move(payload),
              options.is_beginning,
              options.is_end,
              options.is_unordered),
        immediate_ack_(options.immediate_ack) {}

  AnyDataChunk(uint32_t tsn, Data data, bool immediate_ack)
      : tsn_(tsn), data_(std::move(data)), immediate_ack_(immediate_ack) {}

 protected:
  // Bits in `flags` header field.
  static constexpr int kFlagsBitEnd = 0;
  static constexpr int kFlagsBitBeginning = 1;
  static constexpr int kFlagsBitUnordered = 2;
  static constexpr int kFlagsBitImmediateAck = 3;

 private:
  uint32_t tsn_;
  Data data_;
  bool immediate_ack_;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_CHUNK_DATA_COMMON_H_
