/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "logging/rtc_event_log/events/rtc_event_field_extraction.h"
#include "rtc_base/logging.h"

namespace webrtc_event_logging {
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

// To maintain backwards compatibility with past (or future) logs,
// the constants in this enum must not be reordered or changed.
enum class FieldType : uint8_t {
  kFixed8 = 0,
  kFixed32,
  kFixed64,
  kVarInt,
  kString,
};

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
  EventEncoder(EventParameters params, rtc::ArrayView<const RtcEvent*> batch);

  void EncodeField(const FieldParameters& params,
                   const std::vector<uint64_t>& values);

  void EncodeField(const FieldParameters& params,
                   const ValuesWithPositions& values);

  std::string AsString();

 private:
  size_t batch_size_;
  std::string encoded_event_;
  std::vector<std::string> encoded_fields_;
};

class EventParser {
 public:
  EventParser() = default;

  // N.B: This method stores a abls::string_view into the string to be
  // parsed. The caller is responsible for ensuring that the actual string
  // remains unmodified and outlives the EventParser.
  webrtc_event_logging::ParseStatus Initialize(absl::string_view s,
                                               bool batched);

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
#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_
