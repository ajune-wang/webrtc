#include "net/dcsctp/packet/chunk/idata_chunk.h"

#include <stdint.h>

#include <string>
#include <type_traits>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc8260#section-2.1

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |   Type = 64   |  Res  |I|U|B|E|       Length = Variable       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              TSN                              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |        Stream Identifier      |           Reserved            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      Message Identifier                       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |    Payload Protocol Identifier / Fragment Sequence Number     |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  \                                                               \
//  /                           User Data                           /
//  \                                                               \
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int IDataChunk::kType;

absl::optional<IDataChunk> IDataChunk::Parse(
    rtc::ArrayView<const uint8_t> data) {
  absl::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return absl::nullopt;
  }
  uint8_t flags = reader->Load8<1>();
  uint32_t tsn = reader->Load32<4>();
  uint16_t stream_identifier = reader->Load16<8>();
  uint32_t message_id = reader->Load32<12>();
  uint32_t ppid_or_fsn = reader->Load32<16>();
  DataChunkOptions options = {
      .is_end = (flags & (1 << kFlagsBitEnd)) != 0,
      .is_beginning = (flags & (1 << kFlagsBitBeginning)) != 0,
      .is_unordered = (flags & (1 << kFlagsBitUnordered)) != 0,
      .immediate_ack = (flags & (1 << kFlagsBitImmediateAck)) != 0,
  };

  return IDataChunk(tsn, stream_identifier, message_id,
                    /*ppid=*/options.is_beginning ? ppid_or_fsn : 0,
                    /*fsn=*/options.is_beginning ? 0 : ppid_or_fsn,
                    std::vector<uint8_t>(reader->variable_data().begin(),
                                         reader->variable_data().end()),
                    options);
}

void IDataChunk::SerializeTo(std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer = AllocateTLV(out, payload().size());

  writer.Store8<1>(
      (options().is_end ? (1 << kFlagsBitEnd) : 0) |
      (options().is_beginning ? (1 << kFlagsBitBeginning) : 0) |
      (options().is_unordered ? (1 << kFlagsBitUnordered) : 0) |
      (options().immediate_ack ? (1 << kFlagsBitImmediateAck) : 0));
  writer.Store32<4>(tsn());
  writer.Store16<8>(stream_id());
  writer.Store32<12>(message_id());
  writer.Store32<16>(options().is_beginning ? ppid() : fsn());
  writer.CopyToVariableData(payload());
}

std::string IDataChunk::ToString() const {
  rtc::StringBuilder sb;
  sb << "I-DATA, type=" << (options().is_unordered ? "unordered" : "ordered")
     << "::"
     << (options().is_beginning && options().is_end
             ? "complete"
             : options().is_beginning ? "first"
                                      : options().is_end ? "last" : "middle")
     << ", tsn=" << tsn() << ", stream_id=" << stream_id()
     << ", message_id=" << message_id();

  if (options().is_beginning) {
    sb << ", ppid=" << ppid();
  } else {
    sb << ", fsn=" << fsn();
  }
  sb << ", length=" << payload().size();
  return sb.Release();
}

}  // namespace dcsctp
