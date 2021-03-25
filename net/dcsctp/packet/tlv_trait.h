/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PACKET_TLV_TRAIT_H_
#define NET_DCSCTP_PACKET_TLV_TRAIT_H_

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

// Various entities in SCTP are padded data blocks, with an type and length
// field at fixed offsets, all stored in a 4-byte header.
//
// See e.g. https://tools.ietf.org/html/rfc4960#section-3.2 and
// https://tools.ietf.org/html/rfc4960#section-3.2.1
//
// These are helper classes for writing and parsing that data, which in SCTP is
// called Type-Length-Value, or TLV.
//
// This templated class is configurable - a struct passed in as template
// parameter with the following expected members:
//   * kType                    - The type field's value
//   * kTypeSizeInBytes         - The type field's width in bytes.
//                                Either 1 or 2.
//   * kHeaderSize              - The fixed size header
//   * kVariableLengthAlignment - The size alignment on the variable data. Set
//                                to zero (0) if no variable data is used.
//
// This class is to be used as a trait
// (https://en.wikipedia.org/wiki/Trait_(computer_programming)) that adds a few
// public and protected members and which a class inherits from when it
// represents a type-length-value object.
template <typename Config>
class TLVTrait {
 protected:
  static constexpr size_t kHeaderSize = Config::kHeaderSize;

  static_assert(Config::kTypeSizeInBytes == 1 || Config::kTypeSizeInBytes == 2,
                "kTypeSizeInBytes must be 1 or 2");
  static_assert(Config::kHeaderSize >= 4, "HeaderSize must be >= 4 bytes");
  static_assert((Config::kHeaderSize % 4 == 0),
                "kHeaderSize must be even divisable by 4 bytes");
  static_assert((Config::kVariableLengthAlignment == 0 ||
                 Config::kVariableLengthAlignment == 1 ||
                 Config::kVariableLengthAlignment == 2 ||
                 Config::kVariableLengthAlignment == 4 ||
                 Config::kVariableLengthAlignment == 8),
                "kVariableLengthAlignment must be an allowed value");

  // Validates the data with regards to size, alignment and type.
  // If valid, returns a bounded buffer.
  static absl::optional<BoundedByteReader<Config::kHeaderSize>> ParseTLV(
      rtc::ArrayView<const uint8_t> data) {
    if (data.size() < Config::kHeaderSize) {
      RTC_LOG(LS_WARNING) << "Invalid size (" << data.size()
                          << ", expected minimum " << Config::kHeaderSize
                          << " bytes)";
      return absl::nullopt;
    }

    int type;
    if (Config::kTypeSizeInBytes == 1) {
      type = data[0];
    } else {
      type = LoadBigEndian16(&data[0]);
    }
    if (type != Config::kType) {
      RTC_LOG(LS_WARNING) << "Invalid type (" << type << ", expected "
                          << Config::kType << ")";
      return absl::nullopt;
    }
    uint16_t length = LoadBigEndian16(&data[2]);
    if (Config::kVariableLengthAlignment == 0) {
      // Don't expect any variable length data at all.
      if (length != Config::kHeaderSize || data.size() != Config::kHeaderSize) {
        RTC_LOG(LS_WARNING)
            << "Invalid length field (" << length << ", expected "
            << Config::kHeaderSize << " bytes)";
        return absl::nullopt;
      }
    } else {
      // Expect variable length data - verify its size alignment.
      if (length > data.size()) {
        RTC_LOG(LS_WARNING) << "Invalid length field (" << length
                            << ", available " << data.size() << " bytes)";
        return absl::nullopt;
      }
      size_t padding = data.size() - length;
      if (padding > 3) {
        // https://tools.ietf.org/html/rfc4960#section-3.2
        // "This padding MUST NOT be more than 3 bytes in total"
        RTC_LOG(LS_WARNING) << "Invalid padding (" << padding << " bytes)";
        return absl::nullopt;
      }
      if ((length % Config::kVariableLengthAlignment) != 0) {
        RTC_LOG(LS_WARNING) << "Invalid length field (" << length
                            << ", expected even divisable by "
                            << Config::kVariableLengthAlignment << " bytes)";
        return absl::nullopt;
      }
    }
    return BoundedByteReader<Config::kHeaderSize>(data.subview(0, length));
  }

  // Allocates space for data with a static header size, as defined by
  // `Config::kHeaderSize` and a variable footer, as defined by `variable_size`
  // (which may be 0) and writes the type and length in the header.
  static BoundedByteWriter<Config::kHeaderSize> AllocateTLV(
      std::vector<uint8_t>& out,
      size_t variable_size = 0) {
    size_t offset = out.size();
    size_t size = Config::kHeaderSize + variable_size;
    out.resize(offset + size);
    if (Config::kTypeSizeInBytes == 1) {
      out[offset] = Config::kType;
    } else {
      StoreBigEndian16(&out[offset], Config::kType);
    }

    StoreBigEndian16(&out[offset + 2], size);
    return BoundedByteWriter<Config::kHeaderSize>(
        rtc::ArrayView<uint8_t>(out.data() + offset, size));
  }
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_TLV_TRAIT_H_
