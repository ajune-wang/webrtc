/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_EXTRACTION_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_EXTRACTION_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "rtc_base/logging.h"

namespace webrtc_event_logging {
uint64_t UnsignedBitWidth(uint64_t input, bool zero_val_as_zero_width = false);
uint64_t SignedBitWidth(uint64_t max_pos_magnitude, uint64_t max_neg_magnitude);
uint64_t MaxUnsignedValueOfBitWidth(uint64_t bit_width);
uint64_t UnsignedDelta(uint64_t previous, uint64_t current, uint64_t bit_mask);
}  // namespace webrtc_event_logging

namespace webrtc {
template <typename T, std::enable_if_t<std::is_signed<T>::value, bool> = true>
uint64_t EncodeAsUnsigned(T value) {
  return webrtc_event_logging::ToUnsigned(value);
}

template <typename T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
uint64_t EncodeAsUnsigned(T value) {
  return static_cast<uint64_t>(value);
}

template <typename T, std::enable_if_t<std::is_signed<T>::value, bool> = true>
T DecodeFromUnsignedToType(uint64_t value) {
  T signed_value = 0;
  bool success = webrtc_event_logging::ToSigned<T>(value, &signed_value);
  if (!success) {
    RTC_LOG(LS_ERROR) << "Failed to convert " << value << "to signed type.";
    // TODO(terelius): Propagate error?
  }
  return signed_value;
}

template <typename T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
T DecodeFromUnsignedToType(uint64_t value) {
  // TODO(terelius): Check range?
  return static_cast<T>(value);
}

// Given a batch of RtcEvents and a member pointer, extract that
// member from each event in the batch. Signed integer members are
// encoded as unsigned, and the bitsize increased so the result can
// represented as a std::vector<uin64_t>.
// This is intended to be used in conjuction with
// EventEncoder::EncodeField to encode a batch of events as follows:
// auto values = ExtractRtcEventMember(batch, RtcEventFoo::timestamp_ms);
// encoder.EncodeField(timestamp_params, values)
template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
std::vector<uint64_t> ExtractRtcEventMember(
    rtc::ArrayView<const RtcEvent*> batch,
    const T E::*member) {
  constexpr RtcEvent::Type expected_type = E::kType;
  std::vector<uint64_t> values;
  values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_DCHECK_EQ(event->GetType(), expected_type);
    T value = static_cast<const E*>(event)->*member;
    values.push_back(EncodeAsUnsigned(value));
  }
  return values;
}

struct ValuesWithPositions {
  std::vector<bool> positions;
  std::vector<uint64_t> values;
};

// Same as above but for optional fields. It returns a struct
// containing a vector of positions in addition to the vector of values.
// The vector `positions` has the same length as the batch where
// `positions[i] == true` iff the batch[i]->member has a value.
// The values vector only contains the values that exists, so it
// may be shorter than the batch.
template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
ValuesWithPositions ExtractRtcEventMember(rtc::ArrayView<const RtcEvent*> batch,
                                          const absl::optional<T> E::*member) {
  ValuesWithPositions result;
  result.positions.reserve(batch.size());
  result.values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_DCHECK_EQ(event->GetType(), E::kType);
    absl::optional<T> field = static_cast<const E*>(event)->*member;
    if (field.has_value()) {
      result.positions.push_back(true);
      result.values.push_back(EncodeAsUnsigned(field.value()));
    } else {
      result.positions.push_back(false);
    }
  }
  return result;
}

// Inverse of the ExtractRtcEventMember function used when parsing
// a log. Uses a vector of values to populate a specific field in a
// vector of structs.
template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
bool PopulateRtcEventMember(const std::vector<uint64_t>& values,
                            T E::*member,
                            std::vector<E>* output) {
  size_t batch_size = values.size();
  if (output->size() < batch_size)
    return false;
  for (size_t i = 0; i < batch_size; i++) {
    T value = DecodeFromUnsignedToType<T>(values[i]);
    (*output)[output->size() - batch_size + i].*member = value;
  }
  return true;
}

// Same as above, but for optional fields.
template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
bool PopulateRtcEventMember(const std::vector<bool>& positions,
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
      T value = DecodeFromUnsignedToType<T>(value_it);
      (*output)[output->size() - batch_size + i].*member = value;
      ++value_it;
    } else {
      (*output)[output->size() - batch_size + i].*member = absl::nullopt;
    }
  }
  return true;
}

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_EXTRACTION_H_
