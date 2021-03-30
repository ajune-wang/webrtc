#include "net/dcsctp/packet/error_cause/restart_of_an_association_with_new_address_cause.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "net/dcsctp/packet/tlv_trait.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.10.11

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |         Cause Code=11         |      Cause Length=Variable    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  /                       New Address TLVs                        /
//  \                                                               \
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int RestartOfAnAssociationWithNewAddressesCause::kType;

absl::optional<RestartOfAnAssociationWithNewAddressesCause>
RestartOfAnAssociationWithNewAddressesCause::Parse(
    rtc::ArrayView<const uint8_t> data) {
  absl::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return absl::nullopt;
  }
  return RestartOfAnAssociationWithNewAddressesCause(reader->variable_data());
}

void RestartOfAnAssociationWithNewAddressesCause::SerializeTo(
    std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer =
      AllocateTLV(out, new_address_tlvs_.size());
  writer.CopyToVariableData(new_address_tlvs_);
}

std::string RestartOfAnAssociationWithNewAddressesCause::ToString() const {
  return "Restart of an Association with New Addresses";
}

}  // namespace dcsctp
