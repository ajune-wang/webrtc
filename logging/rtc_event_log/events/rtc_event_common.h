/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_COMMON_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_COMMON_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/encoder/bit_writer.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "logging/rtc_event_log/encoder/var_int.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc_event_logging {
uint64_t UnsignedBitWidth(uint64_t input, bool zero_val_as_zero_width = false);
uint64_t SignedBitWidth(uint64_t max_pos_magnitude, uint64_t max_neg_magnitude);
uint64_t MaxUnsignedValueOfBitWidth(uint64_t bit_width);
uint64_t UnsignedDelta(uint64_t previous, uint64_t current, uint64_t bit_mask);
std::string SerializeLittleEndian(uint64_t value, uint8_t bytes);

class ParseStatus {
 public:
  static ParseStatus Success() { return ParseStatus(); }
  static ParseStatus Error(std::string error, std::string file, int line) {
    return ParseStatus(error, file, line);
  }

  bool ok() const { return error_.empty() && file_.empty() && line_ == 0; }
  std::string message() const {
    return error_ + " failed at " + file_ + " line " + std::to_string(line_);
  }

  ABSL_DEPRECATED("") operator bool() const { return ok(); }

 private:
  ParseStatus() : error_(), file_(), line_(0) {}
  ParseStatus(std::string error, std::string file, int line)
      : error_(error), file_(file), line_(line) {}
  std::string error_;
  std::string file_;
  int line_;
};

}  // namespace webrtc_event_logging

namespace webrtc {

// enum class SignTreatment {
//  kUnsigned,  // Field uses an unsigned type.
//  kTwosComplement,  // Field uses a signed data type, but negative values are
//  uncommon or impossible. kZigZag // TODO: Field uses a signed data type, and
//  negative values are common.
//};

// The constants in this enum must not be reordered or changed.
enum class FieldType : uint8_t {
  kFixed8 = 0,
  kFixed32,
  kFixed64,
  kVarInt,
  kString,
};

template <typename T, std::enable_if_t<std::is_signed<T>::value, bool> = true>
uint64_t ConvertToUnsignedIfSigned(T value) {
  return webrtc_event_logging::ToUnsigned(value);
}

template <typename T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
uint64_t ConvertToUnsignedIfSigned(T value) {
  return static_cast<uint64_t>(value);
}

template <typename T, std::enable_if_t<std::is_signed<T>::value, bool> = true>
T ConvertToSignedIfSignedType(uint64_t value) {
  T signed_value = 0;
  bool success = webrtc_event_logging::ToSigned<T>(value, &signed_value);
  if (!success) {
    RTC_LOG(LS_ERROR) << "Failed to convert " << value << "to signed type.";
    // TODO: Handle return value
  }
  return signed_value;
}

template <typename T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
T ConvertToSignedIfSignedType(uint64_t value) {
  // TODO: Check range
  return static_cast<T>(value);
}

template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
std::vector<uint64_t> Extract(rtc::ArrayView<const RtcEvent*> batch,
                              const T E::*member) {
  constexpr RtcEvent::Type expected_type = E::kType;
  std::vector<uint64_t> values;
  values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_DCHECK_EQ(event->GetType(), expected_type);
    T value = static_cast<const E*>(event)->*member;
    values.push_back(ConvertToUnsignedIfSigned(value));
  }
  return values;
}

struct ValuesWithPositions {
  std::vector<bool> positions;
  std::vector<uint64_t> values;
};

template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
ValuesWithPositions Extract(rtc::ArrayView<const RtcEvent*> batch,
                            const absl::optional<T> E::*member) {
  ValuesWithPositions result;
  result.positions.reserve(batch.size());
  result.values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_DCHECK_EQ(event->GetType(), E::kType);
    absl::optional<T> field = static_cast<const E*>(event)->*member;
    if (field.has_value()) {
      result.positions.push_back(true);
      result.values.push_back(ConvertToUnsignedIfSigned(field.value()));
    } else {
      result.positions.push_back(false);
    }
  }
  return result;
}

template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
bool Populate(const std::vector<uint64_t>& values,
              T E::*member,
              std::vector<E>* output) {
  size_t batch_size = values.size();
  if (output->size() < batch_size)
    return false;
  for (size_t i = 0; i < batch_size; i++) {
    T value = ConvertToSignedIfSignedType<T>(values[i]);
    (*output)[output->size() - batch_size + i].*member = value;
  }
  return true;
}

template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
bool Populate(const std::vector<bool>& positions,
              const std::vector<uint64_t>& values,
              absl::optional<T> E::*member,
              std::vector<E>* output) {
  size_t batch_size = positions.size();
  if (output->size() < batch_size || values.size() > batch_size)
    return false;
  auto value_it = values.begin();
  for (size_t i = 0; i < batch_size; i++) {
    if (positions[i]) {
      if (value_it == values.end())
        return false;
      T value = ConvertToSignedIfSignedType<T>(value_it);
      (*output)[output->size() - batch_size + i].*member = value;
      ++value_it;
    } else {
      (*output)[output->size() - batch_size + i].*member = absl::nullopt;
    }
  }
  return true;
}

struct EventParameters {
  constexpr EventParameters(const char* n, RtcEvent::Type i) : name(n), id(i) {}
  const char* name;
  const RtcEvent::Type id;
};

struct FieldParameters {
  constexpr FieldParameters(const char* n, uint64_t i, FieldType t, uint64_t w)
      : name(n), field_id(i), field_type(t), value_width(w) {}
  const char* name;
  const uint64_t field_id;
  const FieldType field_type;
  const uint64_t value_width;
  static constexpr uint64_t kTimestampField = 0;
};

class EventEncoder {
 public:
  EventEncoder(EventParameters params, rtc::ArrayView<const RtcEvent*> batch) {
    batch_size_ = batch.size();
    if (!batch.empty()) {
      // Encode event type.
      uint32_t batched = batch.size() > 1 ? 1 : 0;
      uint32_t event_type = (static_cast<uint32_t>(params.id) << 1) + batched;
      encoded_event_.append(EncodeVarInt(event_type));

      // Number of encoded bytes will be filled in when the encoding is
      // finalized in AsString()

      // Encode number of events in batch
      if (batched) {
        encoded_fields_.push_back(EncodeVarInt(batch.size()));
      }

      // Encode timestamp
      std::vector<uint64_t> timestamps;
      timestamps.reserve(batch.size());
      for (const RtcEvent* event : batch) {
        timestamps.push_back(ConvertToUnsignedIfSigned(event->timestamp_ms()));
      }
      constexpr FieldParameters timestamp_params(
          "timestamp_ms", FieldParameters::kTimestampField, FieldType::kVarInt,
          64);
      EncodeField(timestamp_params, timestamps);
    }
  }

  void EncodeField(const FieldParameters& params,
                   const std::vector<uint64_t>& values);

  void EncodeField(const FieldParameters& params,
                   const ValuesWithPositions& values);

  std::string AsString() {
    if (batch_size_ == 0) {
      RTC_DCHECK_EQ(encoded_event_.size(), 0);
      RTC_DCHECK_EQ(encoded_fields_.size(), 0);
      return std::move(encoded_event_);
    }

    // Compute size of encoded fields.
    size_t event_size = 0;
    for (const std::string& s : encoded_fields_) {
      event_size += s.size();
    }
    encoded_event_.reserve(encoded_event_.size() + 4 + event_size);

    // Encode size.
    encoded_event_.append(EncodeVarInt(event_size));

    // Append encoded fields.
    for (const std::string& s : encoded_fields_) {
      encoded_event_.append(s);
    }

    return std::move(encoded_event_);
  }

 private:
  size_t batch_size_;
  std::string encoded_event_;
  std::vector<std::string> encoded_fields_;
};

// N.B: This class stores a abls::string_view into the string to be
// parsed. The caller is responsible for ensuring that the actual string
// remains unmodified and outlives the EventParser.
class EventParser {
 public:
  EventParser() = default;

  webrtc_event_logging::ParseStatus Initialize(absl::string_view s,
                                               bool batched) {
    s_ = s;
    batched_ = batched;
    num_events_ = 1;

    bool success = false;
    if (batched_) {
      std::tie(success, s_) = DecodeVarInt(s_, &num_events_);
      if (!success) {
        return webrtc_event_logging::ParseStatus::Error(
            "Failed to read number of events in batch.", __FILE__, __LINE__);
      }
    }
    return webrtc_event_logging::ParseStatus::Success();
  }

  webrtc_event_logging::ParseStatus ParseField(const FieldParameters& params,
                                               std::vector<uint64_t>* values);

  webrtc_event_logging::ParseStatus ParseField(const FieldParameters& params,
                                               std::vector<bool>* positions,
                                               std::vector<uint64_t>* values);

  uint64_t num_events() const { return num_events_; }
  size_t remaining_bytes() const { return s_.size(); }

 private:
  absl::string_view s_;
  bool batched_;
  uint64_t num_events_ = 1;
  uint64_t last_field_id_ = FieldParameters::kTimestampField;
};

// Parameters for fixed-size delta-encoding/decoding.
// These are tailored for the sequence which will be encoded (e.g. widths).
class FixedLengthEncodingParametersV3 final {
 public:
  static bool ValidParameters(uint64_t delta_width_bits,
                              bool signed_deltas,
                              bool values_optional,
                              uint64_t value_width_bits) {
    return (1 <= delta_width_bits && delta_width_bits <= 64 &&
            1 <= value_width_bits && value_width_bits <= 64 &&
            (delta_width_bits <= value_width_bits ||
             (signed_deltas && delta_width_bits == 64)));
  }

  static FixedLengthEncodingParametersV3 EqualValues(
      bool values_optional,
      uint64_t value_width_bits) {
    return FixedLengthEncodingParametersV3(/*delta_width_bits=*/64,
                                           /*signed_deltas=*/true,
                                           values_optional, value_width_bits);
  }

  static FixedLengthEncodingParametersV3 CalculateParameters(
      uint64_t base,
      const rtc::ArrayView<const uint64_t> values,
      uint64_t value_width_bits,
      bool values_optional);
  static absl::optional<FixedLengthEncodingParametersV3> ParseDeltaHeader(
      uint64_t header,
      uint64_t value_width_bits);

  uint64_t DeltaHeaderAsInt() const;

  // Number of bits necessary to hold the widest(*) of the deltas between the
  // values in the sequence.
  // (*) - Widest might not be the largest, if signed deltas are used.
  uint64_t delta_width_bits() const { return delta_width_bits_; }

  // Whether deltas are signed.
  bool signed_deltas() const { return signed_deltas_; }

  // Whether the values of the sequence are optional. That is, it may be
  // that some of them do not have a value (not even a sentinel value indicating
  // invalidity).
  bool values_optional() const { return values_optional_; }

  // Whether all values are equal. 64-bit signed deltas are assumed to not
  // occur, since those could equally well be represented using 64 bit unsigned
  // deltas.
  bool values_equal() const {
    return delta_width_bits() == 64 && signed_deltas();
  }

  // Number of bits necessary to hold the largest value in the sequence.
  uint64_t value_width_bits() const { return value_width_bits_; }

  // Masks where only the bits relevant to the deltas/values are turned on.
  uint64_t delta_mask() const { return delta_mask_; }
  uint64_t value_mask() const { return value_mask_; }

 private:
  FixedLengthEncodingParametersV3(uint64_t delta_width_bits,
                                  bool signed_deltas,
                                  bool values_optional,
                                  uint64_t value_width_bits)
      : delta_width_bits_(delta_width_bits),
        signed_deltas_(signed_deltas),
        values_optional_(values_optional),
        value_width_bits_(value_width_bits),
        delta_mask_(webrtc_event_logging::MaxUnsignedValueOfBitWidth(
            delta_width_bits_)),
        value_mask_(webrtc_event_logging::MaxUnsignedValueOfBitWidth(
            value_width_bits_)) {}

  uint64_t delta_width_bits_;
  bool signed_deltas_;
  bool values_optional_;
  uint64_t value_width_bits_;

  uint64_t delta_mask_;
  uint64_t value_mask_;
};

std::string EncodeSingleValue(uint64_t value, FieldType field_type);
std::string EncodeDeltasV3(FixedLengthEncodingParametersV3 params,
                           uint64_t base,
                           rtc::ArrayView<const uint64_t> values);

}  // namespace webrtc
#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_COMMON_H_
