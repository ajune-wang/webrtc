/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_common.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/encoder/bit_writer.h"
#include "logging/rtc_event_log/encoder/var_int.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc_event_logging {
// TODO(terelius): Replace by something more efficient.
uint64_t UnsignedBitWidth(uint64_t input, bool zero_val_as_zero_width) {
  if (zero_val_as_zero_width && input == 0) {
    return 0;
  }

  uint64_t width = 0;
  do {  // input == 0 -> width == 1
    width += 1;
    input >>= 1;
  } while (input != 0);
  return width;
}

// New (logarithmic) version
// uint64_t UnsignedBitWidth(uint64_t x, bool zero_val_as_zero_width) {

//  uint64_t log = 0;
//  constexpr uint64_t masks[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
//  constexpr uint64_t shifts[] = {1, 2, 4, 8, 16, 32};

//  // Check if anything is set in the top 32 bits of the 64 bit input,
//  // then check top 16 bits of the remaining 32 bit word,
//  // then top 8 bits of the remaining 16 bit half-word and so on.
//  for (int i = 5; i >= 0; i--) {
//    if (x & masks[i]) {
//      x >>= shifts[i];
//      width |= shifts[i];
//    }
//  }

//  if (zero_val_as_zero_width) {
//    // `x` is now either 0 (if the original input was 0) or 1 (otherwise).
//    return log + x;
//  }
//  return log + 1;
//}

uint64_t SignedBitWidth(uint64_t max_pos_magnitude,
                        uint64_t max_neg_magnitude) {
  const uint64_t bitwidth_pos = UnsignedBitWidth(max_pos_magnitude, true);
  const uint64_t bitwidth_neg =
      (max_neg_magnitude > 0) ? UnsignedBitWidth(max_neg_magnitude - 1, true)
                              : 0;
  return 1 + std::max(bitwidth_pos, bitwidth_neg);
}

// Return the maximum integer of a given bit width.
// Examples:
// MaxUnsignedValueOfBitWidth(1) = 0x01
// MaxUnsignedValueOfBitWidth(6) = 0x3f
// MaxUnsignedValueOfBitWidth(8) = 0xff
// MaxUnsignedValueOfBitWidth(32) = 0xffffffff
uint64_t MaxUnsignedValueOfBitWidth(uint64_t bit_width) {
  RTC_DCHECK_GE(bit_width, 1);
  RTC_DCHECK_LE(bit_width, 64);
  return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
                           : ((static_cast<uint64_t>(1) << bit_width) - 1);
}

// Computes the delta between |previous| and |current|, under the assumption
// that wrap-around occurs after |width| is exceeded.
uint64_t UnsignedDelta(uint64_t previous, uint64_t current, uint64_t bit_mask) {
  RTC_DCHECK_LE(previous, bit_mask);
  RTC_DCHECK_LE(current, bit_mask);
  return (current - previous) & bit_mask;
}

std::string SerializeLittleEndian(uint64_t value, uint8_t bytes) {
  RTC_DCHECK_LE(bytes, sizeof(uint64_t));
  RTC_DCHECK_GE(bytes, 1);
  if (bytes < sizeof(uint64_t)) {
    // Note that shifting a 64-bit value by 64 (or more) bits is undefined.
    RTC_DCHECK_EQ(value >> (8 * bytes), 0);
  }
  std::string output(bytes, 0);
  uint8_t* p = reinterpret_cast<uint8_t*>(&output[0]);
  //#ifdef WEBRTC_ARCH_LITTLE_ENDIAN
  // TODO memcpy(p, &value, bytes);
  //#else
  while (bytes > 0) {
    *p = static_cast<uint8_t>(value & 0xFF);
    value >>= 8;
    ++p;
    --bytes;
  }
  //#endif //WEBRTC_ARCH_LITTLE_ENDIAN
  return output;
}

std::pair<bool, absl::string_view> ParseLittleEndian(absl::string_view s,
                                                     uint8_t bytes,
                                                     uint64_t* output) {
  RTC_DCHECK_LE(bytes, sizeof(uint64_t));
  RTC_DCHECK_GE(bytes, 1);
  RTC_DCHECK(output != nullptr);

  if (bytes > s.length()) {
    return std::make_pair(false, s);
  }

  const uint8_t* p = reinterpret_cast<const uint8_t*>(s.data());
  unsigned int shift = 0;
  uint64_t value = 0;
  uint8_t remaining = bytes;
  while (remaining > 0) {
    value += (static_cast<uint64_t>(*p) << shift);
    shift += 8;
    ++p;
    --remaining;
  }

  *output = value;
  return std::make_pair(true, s.substr(bytes));
}

}  // namespace webrtc_event_logging

namespace webrtc {

std::string EncodeOptionalValuePositions(std::vector<bool> positions) {
  BitWriter writer((positions.size() + 7) / 8);
  for (bool position : positions) {
    writer.WriteBits(position ? 1u : 0u, 1);
  }
  return writer.GetString();
}

std::pair<bool, absl::string_view> DecodeOptionalValuePositions(
    absl::string_view s,
    uint64_t num_deltas,
    std::vector<bool>* positions) {
  if (s.size() * 8 < num_deltas) {
    return std::make_pair(false, s);
  }

  rtc::BitBuffer reader(reinterpret_cast<const uint8_t*>(s.data()), s.size());
  for (size_t i = 0; i < num_deltas; i++) {
    uint64_t bit;
    if (!reader.ReadBits(&bit, 1)) {
      return std::make_pair(false, s);
    }
    positions->push_back(bit);
  }
  return std::make_pair(true, s.substr((num_deltas + 7) / 8));
}

std::string EncodeSingleValue(uint64_t value, FieldType field_type) {
  switch (field_type) {
    case FieldType::kFixed8:
      return webrtc_event_logging::SerializeLittleEndian(value, 1 /*byte*/);
    case FieldType::kFixed32:
      return webrtc_event_logging::SerializeLittleEndian(value, 4 /*bytes*/);
    case FieldType::kFixed64:
      return webrtc_event_logging::SerializeLittleEndian(value, 8 /*bytes*/);
    case FieldType::kVarInt:
      return EncodeVarInt(value);
    case FieldType::kString:
      RTC_NOTREACHED();
      return std::string();
  }
}

std::pair<bool, absl::string_view> ParseSingleValue(absl::string_view s,
                                                    FieldType field_type,
                                                    uint64_t* output) {
  switch (field_type) {
    case FieldType::kFixed8:
      return webrtc_event_logging::ParseLittleEndian(s, 1 /*byte*/, output);
    case FieldType::kFixed32:
      return webrtc_event_logging::ParseLittleEndian(s, 4 /*bytes*/, output);
    case FieldType::kFixed64:
      return webrtc_event_logging::ParseLittleEndian(s, 8 /*bytes*/, output);
    case FieldType::kVarInt:
      return DecodeVarInt(s, output);
    case FieldType::kString:
      RTC_NOTREACHED();
      return std::make_pair(false, s);
  }
}

absl::optional<FieldType> ConvertFieldType(uint64_t value) {
  switch (value) {
    case static_cast<uint64_t>(FieldType::kFixed8):
      return FieldType::kFixed8;
    case static_cast<uint64_t>(FieldType::kFixed32):
      return FieldType::kFixed32;
    case static_cast<uint64_t>(FieldType::kFixed64):
      return FieldType::kFixed64;
    case static_cast<uint64_t>(FieldType::kVarInt):
      return FieldType::kVarInt;
    case static_cast<uint64_t>(FieldType::kString):
      return FieldType::kString;
    default:
      return absl::nullopt;
  }
}

std::string EncodeDeltasV3(FixedLengthEncodingParametersV3 params,
                           uint64_t base,
                           rtc::ArrayView<const uint64_t> values) {
  size_t outputbound = (values.size() * params.delta_width_bits() + 7) / 8;
  BitWriter writer(outputbound);

  uint64_t previous = base;
  for (uint64_t value : values) {
    if (params.signed_deltas()) {
      const uint64_t forward_delta = webrtc_event_logging::UnsignedDelta(
          previous, value, params.value_mask());
      const uint64_t backward_delta = webrtc_event_logging::UnsignedDelta(
          value, previous, params.value_mask());
      uint64_t delta;
      if (forward_delta <= backward_delta) {
        delta = forward_delta;
      } else {
        // Compute the unsigned representation of a negative delta.
        // This is the two's complement representation of this negative value,
        // when deltas are of width params_.delta_mask().
        RTC_DCHECK_GE(params.delta_mask(), backward_delta);
        RTC_DCHECK_LT(params.delta_mask() - backward_delta,
                      params.delta_mask());
        delta = params.delta_mask() - backward_delta + 1;
        RTC_DCHECK_LE(delta, params.delta_mask());
      }
      writer.WriteBits(delta, params.delta_width_bits());
    } else {
      const uint64_t delta = webrtc_event_logging::UnsignedDelta(
          previous, value, params.value_mask());
      writer.WriteBits(delta, params.delta_width_bits());
    }
    previous = value;
  }

  return writer.GetString();
}

std::pair<bool, absl::string_view> DecodeDeltasV3(
    FixedLengthEncodingParametersV3 params,
    uint64_t num_deltas,
    uint64_t base,
    absl::string_view s,
    std::vector<uint64_t>* values) {
  if (s.size() * 8 < num_deltas * params.delta_width_bits()) {
    return std::make_pair(false, s);
  }

  rtc::BitBuffer reader(reinterpret_cast<const uint8_t*>(s.data()), s.size());
  const uint64_t top_bit = static_cast<uint64_t>(1)
                           << (params.delta_width_bits() - 1);

  for (uint64_t i = 0; i < num_deltas; ++i) {
    uint64_t delta;
    if (!reader.ReadBits(&delta, params.delta_width_bits())) {
      return std::make_pair(false, s);
    }
    RTC_DCHECK_LE(base, webrtc_event_logging::MaxUnsignedValueOfBitWidth(
                            params.value_width_bits()));
    RTC_DCHECK_LE(delta, webrtc_event_logging::MaxUnsignedValueOfBitWidth(
                             params.delta_width_bits()));
    const bool positive_delta = ((delta & top_bit) == 0);
    if (params.signed_deltas() && !positive_delta) {
      const uint64_t delta_abs = (~delta & params.delta_mask()) + 1;
      base = (base - delta_abs) & params.value_mask();
    } else {
      base = (base + delta) & params.value_mask();
    }
    values->push_back(base);
  }
  return std::make_pair(
      true, s.substr((num_deltas * params.delta_width_bits() + 7) / 8));
}

FixedLengthEncodingParametersV3
FixedLengthEncodingParametersV3::CalculateParameters(
    uint64_t base,
    const rtc::ArrayView<const uint64_t> values,
    uint64_t value_width_bits,
    bool values_optional) {
  const uint64_t bit_mask =
      webrtc_event_logging::MaxUnsignedValueOfBitWidth(value_width_bits);

  uint64_t max_unsigned_delta = 0;
  uint64_t max_pos_signed_delta = 0;
  uint64_t min_neg_signed_delta = 0;
  uint64_t prev = base;
  for (size_t i = 0; i < values.size(); ++i) {
    const uint64_t current = values[i];
    const uint64_t forward_delta =
        webrtc_event_logging::UnsignedDelta(prev, current, bit_mask);
    const uint64_t backward_delta =
        webrtc_event_logging::UnsignedDelta(current, prev, bit_mask);

    max_unsigned_delta = std::max(max_unsigned_delta, forward_delta);

    if (forward_delta < backward_delta) {
      max_pos_signed_delta = std::max(max_pos_signed_delta, forward_delta);
    } else {
      min_neg_signed_delta = std::max(min_neg_signed_delta, backward_delta);
    }

    prev = current;
  }

  const uint64_t delta_width_bits_unsigned =
      webrtc_event_logging::UnsignedBitWidth(max_unsigned_delta);
  const uint64_t delta_width_bits_signed = webrtc_event_logging::SignedBitWidth(
      max_pos_signed_delta, min_neg_signed_delta);

  // Note: Preference for unsigned if the two have the same width (efficiency).
  bool signed_deltas = delta_width_bits_signed < delta_width_bits_unsigned;
  uint64_t delta_width_bits =
      signed_deltas ? delta_width_bits_signed : delta_width_bits_unsigned;

  // signed_deltas && delta_width_bits==64 is reserved for "all values equal".
  RTC_DCHECK(!signed_deltas || delta_width_bits < 64);

  RTC_DCHECK(ValidParameters(delta_width_bits, signed_deltas, values_optional,
                             value_width_bits));
  return FixedLengthEncodingParametersV3(delta_width_bits, signed_deltas,
                                         values_optional, value_width_bits);
}

uint64_t FixedLengthEncodingParametersV3::DeltaHeaderAsInt() const {
  uint64_t header = delta_width_bits_ - 1;
  RTC_CHECK_LT(header, 1u << 6);
  if (signed_deltas_) {
    header += 1u << 6;
  }
  RTC_CHECK_LT(header, 1u << 7);
  if (values_optional_) {
    header += 1u << 7;
  }
  return header;
}

absl::optional<FixedLengthEncodingParametersV3>
FixedLengthEncodingParametersV3::ParseDeltaHeader(uint64_t header,
                                                  uint64_t value_width_bits) {
  uint64_t delta_width_bits = (header & ((1u << 6) - 1)) + 1;
  bool signed_deltas = header & (1u << 6);
  bool values_optional = header & (1u << 7);

  if (header >= (1u << 8)) {
    RTC_LOG(LS_ERROR) << "Failed to parse delta header; unread bits remaining.";
    return absl::nullopt;
  }

  if (!ValidParameters(delta_width_bits, signed_deltas, values_optional,
                       value_width_bits)) {
    RTC_LOG(LS_ERROR) << "Failed to parse delta header. Invalid combination of "
                         "values: delta_width_bits="
                      << delta_width_bits << " signed_deltas=" << signed_deltas
                      << " values_optional=" << values_optional
                      << " value_width_bits=" << value_width_bits;
    return absl::nullopt;
  }

  return FixedLengthEncodingParametersV3(delta_width_bits, signed_deltas,
                                         values_optional, value_width_bits);
}

void EventEncoder::EncodeField(const FieldParameters& params,
                               const std::vector<uint64_t>& values) {
  RTC_DCHECK_EQ(values.size(), batch_size_);

  if (values.size() == 0) {
    return;
  }

  if (params.field_id != FieldParameters::kTimestampField) {
    RTC_DCHECK_LE(params.field_id, 1000000);
    uint64_t field_tag = params.field_id << 3;
    field_tag += static_cast<uint64_t>(params.field_type);
    encoded_fields_.push_back(EncodeVarInt(field_tag));
    // fprintf(stderr, "Adding field tag %lu\n", field_tag);
  }

  if (batch_size_ == 1) {
    encoded_fields_.push_back(EncodeSingleValue(values[0], params.field_type));
    // fprintf(stderr, "Adding single value %lu\n", values[0]);
    return;
  }

  // Compute delta parameters
  rtc::ArrayView<const uint64_t> all_values(values);
  uint64_t base = values[0];
  rtc::ArrayView<const uint64_t> remaining_values(all_values.subview(1));

  // As a special case, if all of the elements are identical to the base
  // we just encode the base value with a special delta header.
  if (std::all_of(values.cbegin(), values.cend(),
                  [base](uint64_t val) { return val == base; })) {
    // Delta header with signed=true and delta_bitwidth=64
    FixedLengthEncodingParametersV3 delta_params =
        FixedLengthEncodingParametersV3::EqualValues(/*values_optional=*/false,
                                                     params.value_width);
    encoded_fields_.push_back(EncodeVarInt(delta_params.DeltaHeaderAsInt()));
    // fprintf(stderr, "Adding all-equal delta header %lu\n",
    // delta_params.DeltaHeaderAsInt());

    // TODO: if optional, list of bit positions Update: I.e. we special case
    // also if not all events have the field, but all that do are equal.

    // Base element, encoded as uint8, uint32, uint64 or varint
    encoded_fields_.push_back(EncodeSingleValue(base, params.field_type));
    // fprintf(stderr, "Adding base %lu\n", base);
    return;
  }

  FixedLengthEncodingParametersV3 delta_params =
      FixedLengthEncodingParametersV3::CalculateParameters(
          base, remaining_values, params.value_width,
          /*values_optional*/ false);  // TODO: for absl::optional values, check
                                       // whether all exists

  encoded_fields_.push_back(EncodeVarInt(delta_params.DeltaHeaderAsInt()));
  // fprintf(stderr, "Adding delta header %lu\n",
  // delta_params.DeltaHeaderAsInt());

  //  if (values_optional) {
  //    encoded_field.append(EncodeOptionalValuePositions(batch, accessor));
  //  }
  encoded_fields_.push_back(EncodeSingleValue(base, params.field_type));
  // fprintf(stderr, "Adding base %lu\n", base);

  encoded_fields_.push_back(
      EncodeDeltasV3(delta_params, base, remaining_values));
  // fprintf(stderr, "Adding delta bytes\n");
}

void EventEncoder::EncodeField(const FieldParameters& params,
                               const ValuesWithPositions& vp) {
  RTC_DCHECK_EQ(vp.positions.size(), batch_size_);
  RTC_DCHECK_LE(vp.values.size(), batch_size_);

  if (vp.values.size() == 0) {
    // If all values for a particular field is empty/nullopt,
    // then we completely skip the field even if the the batch is non-empty.
    return;
  }

  if (params.field_id != FieldParameters::kTimestampField) {
    uint64_t field_tag = params.field_id
                         << 3;  // TODO: Ensure this does not overflow
    field_tag += static_cast<uint64_t>(params.field_type);
    encoded_fields_.push_back(EncodeVarInt(field_tag));
    // fprintf(stderr, "Adding field tag %lu\n", field_tag);
  }

  if (batch_size_ == 1) {
    RTC_DCHECK_EQ(vp.values.size(), 1);
    encoded_fields_.push_back(
        EncodeSingleValue(vp.values[0], params.field_type));
    // fprintf(stderr, "Adding single value %lu\n", values[0]);
    return;
  }

  const bool values_optional = vp.values.size() != batch_size_;

  // Compute delta parameters
  rtc::ArrayView<const uint64_t> all_values(vp.values);
  uint64_t base = vp.values[0];
  rtc::ArrayView<const uint64_t> remaining_values(all_values.subview(1));

  // As a special case, if all of the elements are identical to the base
  // we just encode the base value with a special delta header.
  if (std::all_of(vp.values.cbegin(), vp.values.cend(),
                  [base](uint64_t val) { return val == base; })) {
    // Delta header with signed=true and delta_bitwidth=64
    FixedLengthEncodingParametersV3 delta_params =
        FixedLengthEncodingParametersV3::EqualValues(values_optional,
                                                     params.value_width);
    encoded_fields_.push_back(EncodeVarInt(delta_params.DeltaHeaderAsInt()));
    // fprintf(stderr, "Adding all-equal delta header %lu\n",
    // delta_params.DeltaHeaderAsInt());

    if (values_optional) {
      encoded_fields_.push_back(EncodeOptionalValuePositions(vp.positions));
      // fprintf(stderr, "Adding bit positions\n");
    }

    // Base element, encoded as uint8, uint32, uint64 or varint
    encoded_fields_.push_back(EncodeSingleValue(base, params.field_type));
    // fprintf(stderr, "Adding base %lu\n", base);
    return;
  }

  FixedLengthEncodingParametersV3 delta_params =
      FixedLengthEncodingParametersV3::CalculateParameters(
          base, remaining_values, params.value_width, values_optional);

  encoded_fields_.push_back(EncodeVarInt(delta_params.DeltaHeaderAsInt()));
  // fprintf(stderr, "Adding delta header %lu\n",
  // delta_params.DeltaHeaderAsInt());

  if (values_optional) {
    encoded_fields_.push_back(EncodeOptionalValuePositions(vp.positions));
    // fprintf(stderr, "Adding bit positions\n");
  }

  encoded_fields_.push_back(EncodeSingleValue(base, params.field_type));
  // fprintf(stderr, "Adding base %lu\n", base);

  encoded_fields_.push_back(
      EncodeDeltasV3(delta_params, base, remaining_values));
  // fprintf(stderr, "Adding delta bytes\n");
}

webrtc_event_logging::ParseStatus EventParser::ParseField(
    const FieldParameters& params,
    std::vector<uint64_t>* values) {
  //  fprintf(stderr, "Attempting to parse %s, (field id %lu)\n", params.name,
  //          params.field_id);
  //  fprintf(stderr, "from ");
  //  for (auto c : s_) {
  //    fprintf(stderr, "%d ", static_cast<uint8_t>(c));
  //  }
  //  fprintf(stderr, "\n");

  // Verify that the event parses fields in increasing order.
  if (params.field_id == FieldParameters::kTimestampField) {
    RTC_DCHECK_EQ(last_field_id_, FieldParameters::kTimestampField);
  } else {
    RTC_DCHECK_GT(params.field_id, last_field_id_);
  }
  last_field_id_ = params.field_id;

  // Initialization for positional fields that don't encode field ID and type.
  uint64_t field_id = params.field_id;
  FieldType field_type = params.field_type;
  bool success;

  while (!s_.empty()) {
    absl::string_view field_start = s_;
    // Read tag for non-positional fields.
    if (params.field_id != FieldParameters::kTimestampField) {
      uint64_t field_tag;
      std::tie(success, s_) = DecodeVarInt(s_, &field_tag);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to read field tag", __FILE__, __LINE__);
      // Field ID.
      field_id = field_tag >> 3;
      // Field type.
      //      fprintf(stderr, "Field tag %lu, id %lu, type %lu\n", field_tag,
      //      field_id,
      //              field_tag & 7u);
      absl::optional<FieldType> conversion = ConvertFieldType(field_tag & 7u);
      if (!conversion.has_value())
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to parse field type", __FILE__, __LINE__);
      field_type = conversion.value();
    }

    if (field_id > params.field_id) {
      // We've passed all fields with ids less than or equal to what we are
      // looking for. Reset s_ to first field with id higher than
      // params.field_id, since we didn't find the field we were looking for.
      s_ = field_start;
      values->clear();
      return webrtc_event_logging::ParseStatus::Success();
    }

    values->clear();
    if (!batched_) {
      uint64_t base;
      std::tie(success, s_) = ParseSingleValue(s_, field_type, &base);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error("Failed to read value",
                                                        __FILE__, __LINE__);
      values->push_back(base);
    } else {
      // Read delta header.
      uint64_t header_value;
      std::tie(success, s_) = DecodeVarInt(s_, &header_value);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to read delta header", __FILE__, __LINE__);
      // NB: value_width may be incorrect for the field, if this isn't the field
      // we are looking for.
      absl::optional<FixedLengthEncodingParametersV3> delta_header =
          FixedLengthEncodingParametersV3::ParseDeltaHeader(header_value,
                                                            params.value_width);
      if (!delta_header.has_value()) {
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to parse delta header", __FILE__, __LINE__);
      }
      //      fprintf(stderr, "Delta header %u, %u, %lu, %s equal\n",
      //              delta_header.value().values_optional(),
      //              delta_header.value().signed_deltas(),
      //              delta_header.value().delta_width_bits(),
      //              delta_header.value().values_equal() ? "all" : "not");

      uint64_t num_existing_deltas = num_events() - 1;
      if (delta_header->values_optional()) {
        std::vector<bool> positions;
        positions.reserve(num_events());
        std::tie(success, s_) =
            DecodeOptionalValuePositions(s_, num_events(), &positions);
        if (!success) {
          return webrtc_event_logging::ParseStatus::Error(
              "Failed to read bit positions", __FILE__, __LINE__);
        }
        size_t num_nonempty_values =
            std::count(positions.begin(), positions.end(), true);
        if (num_nonempty_values < 1 || num_events() < num_nonempty_values) {
          return webrtc_event_logging::ParseStatus::Error(
              "Expected at least one non_empty values", __FILE__, __LINE__);
        }
        num_existing_deltas = num_nonempty_values - 1;
        //        fprintf(stderr, "Bit positions indicates %lu deltas\n",
        //                num_existing_deltas);
      }

      // Read base.
      uint64_t base;
      std::tie(success, s_) = ParseSingleValue(s_, field_type, &base);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error("Failed to read value",
                                                        __FILE__, __LINE__);
      //      fprintf(stderr, "Read base %lu\n", base);

      values->push_back(base);

      if (delta_header->values_equal()) {
        // Duplicate the base value num_existing_deltas times.
        values->insert(values->end(), num_existing_deltas, base);
      } else {
        // Read deltas; ceil(num_existing_deltas*delta_width/8) bits
        std::tie(success, s_) = DecodeDeltasV3(
            delta_header.value(), num_existing_deltas, base, s_, values);
        if (!success) {
          return webrtc_event_logging::ParseStatus::Error(
              "Failed to decode deltas", __FILE__, __LINE__);
        }
      }
    }

    if (field_id == params.field_id) {
      // The field we we're looking for has been found and values populated.
      return webrtc_event_logging::ParseStatus::Success();
    }
  }

  // Field not found because the event ended.
  values->clear();  // TODO: Do we signal a missing field by clearing values?
  return webrtc_event_logging::ParseStatus::Success();
}

webrtc_event_logging::ParseStatus EventParser::ParseField(
    const FieldParameters& params,
    std::vector<bool>* positions,
    std::vector<uint64_t>* values) {
  //  fprintf(stderr, "Attempting to parse %s, (field id %lu)\n", params.name,
  //          params.field_id);
  //  fprintf(stderr, "from ");
  //  for (auto c : s_) {
  //    fprintf(stderr, "%d ", static_cast<uint8_t>(c));
  //  }
  //  fprintf(stderr, "\n");

  // Verify that the event parses fields in increasing order.
  if (params.field_id == FieldParameters::kTimestampField) {
    RTC_DCHECK_EQ(last_field_id_, FieldParameters::kTimestampField);
  } else {
    RTC_DCHECK_GT(params.field_id, last_field_id_);
  }
  last_field_id_ = params.field_id;

  // Initialization for positional fields that don't encode field ID and type.
  uint64_t field_id = params.field_id;
  FieldType field_type = params.field_type;
  bool success;

  while (!s_.empty()) {
    absl::string_view field_start = s_;
    // Read tag for non-positional fields.
    if (params.field_id != FieldParameters::kTimestampField) {
      uint64_t field_tag;
      std::tie(success, s_) = DecodeVarInt(s_, &field_tag);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to read field tag", __FILE__, __LINE__);
      // Field ID.
      field_id = field_tag >> 3;
      // Field type.
      //      fprintf(stderr, "Field tag %lu, id %lu, type %lu\n", field_tag,
      //      field_id,
      //              field_tag & 7u);
      absl::optional<FieldType> conversion = ConvertFieldType(field_tag & 7u);
      if (!conversion.has_value())
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to parse field type", __FILE__, __LINE__);
      field_type = conversion.value();
    }

    if (field_id > params.field_id) {
      // We've passed all fields with ids less than or equal to what we are
      // looking for. Reset s_ to first field with id higher than
      // params.field_id, since we didn't find the field we were looking for.
      s_ = field_start;
      values->clear();
      positions->clear();
      return webrtc_event_logging::ParseStatus::Success();
    }

    values->clear();
    positions->clear();
    if (!batched_) {
      uint64_t base;
      std::tie(success, s_) = ParseSingleValue(s_, field_type, &base);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error("Failed to read value",
                                                        __FILE__, __LINE__);
      positions->push_back(true);
      values->push_back(base);
    } else {
      // Read delta header.
      uint64_t header_value;
      std::tie(success, s_) = DecodeVarInt(s_, &header_value);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to read delta header", __FILE__, __LINE__);
      // NB: value_width may be incorrect for the field, if this isn't the field
      // we are looking for.
      absl::optional<FixedLengthEncodingParametersV3> delta_header =
          FixedLengthEncodingParametersV3::ParseDeltaHeader(header_value,
                                                            params.value_width);
      if (!delta_header.has_value()) {
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to parse delta header", __FILE__, __LINE__);
      }
      //      fprintf(stderr, "Delta header %u, %u, %lu, %s equal\n",
      //              delta_header.value().values_optional(),
      //              delta_header.value().signed_deltas(),
      //              delta_header.value().delta_width_bits(),
      //              delta_header.value().values_equal() ? "all" : "not");

      uint64_t num_existing_deltas = num_events() - 1;
      if (delta_header->values_optional()) {
        std::tie(success, s_) =
            DecodeOptionalValuePositions(s_, num_events(), positions);
        if (!success) {
          return webrtc_event_logging::ParseStatus::Error(
              "Failed to read bit positions", __FILE__, __LINE__);
        }
        size_t num_nonempty_values =
            std::count(positions->begin(), positions->end(), true);
        if (num_nonempty_values < 1 || num_events() < num_nonempty_values) {
          return webrtc_event_logging::ParseStatus::Error(
              "Expected at least one non_empty values", __FILE__, __LINE__);
        }
        num_existing_deltas = num_nonempty_values - 1;
        //        fprintf(stderr, "Bit positions indicates %lu deltas\n",
        //                num_existing_deltas);
      } else {
        positions->assign(num_events(), true);
      }

      // Read base.
      uint64_t base;
      std::tie(success, s_) = ParseSingleValue(s_, field_type, &base);
      if (!success)
        return webrtc_event_logging::ParseStatus::Error("Failed to read value",
                                                        __FILE__, __LINE__);
      //      fprintf(stderr, "Read base %lu\n", base);

      values->push_back(base);

      if (delta_header->values_equal()) {
        // Duplicate the base value num_existing_deltas times.
        values->insert(values->end(), num_existing_deltas, base);
      } else {
        // Read deltas; ceil(num_existing_deltas*delta_width/8) bits
        std::tie(success, s_) = DecodeDeltasV3(
            delta_header.value(), num_existing_deltas, base, s_, values);
        if (!success) {
          return webrtc_event_logging::ParseStatus::Error(
              "Failed to decode deltas", __FILE__, __LINE__);
        }
      }
    }

    if (field_id == params.field_id) {
      // The field we we're looking for has been found and values populated.
      return webrtc_event_logging::ParseStatus::Success();
    }
  }

  // Field not found because the event ended.
  values->clear();
  positions->clear();
  return webrtc_event_logging::ParseStatus::Success();
}

}  // namespace webrtc
