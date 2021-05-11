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

#include <memory>
#include <string>

#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/events/rtc_event_common.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int32_t kInt32Max = std::numeric_limits<int32_t>::max();
constexpr int32_t kInt32Min = std::numeric_limits<int32_t>::min();
constexpr uint32_t kUint32Max = std::numeric_limits<uint32_t>::max();
constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::min();
constexpr uint64_t kUint64Max = std::numeric_limits<uint64_t>::max();
}  // namespace

class RtcTestEvent final : public RtcEvent {
 public:
  RtcTestEvent(bool b,
               int32_t signed32,
               uint32_t unsigned32,
               int64_t signed64,
               uint64_t unsigned64)
      : b_(b),
        signed32_(signed32),
        unsigned32_(unsigned32),
        signed64_(signed64),
        unsigned64_(unsigned64) {}
  RtcTestEvent(bool b,
               int32_t signed32,
               uint32_t unsigned32,
               int64_t signed64,
               uint64_t unsigned64,
               absl::optional<int32_t> optional_signed32,
               absl::optional<int64_t> optional_signed64,
               uint32_t wrapping21)
      : b_(b),
        signed32_(signed32),
        unsigned32_(unsigned32),
        signed64_(signed64),
        unsigned64_(unsigned64),
        optional_signed32_(optional_signed32),
        optional_signed64_(optional_signed64),
        wrapping21_(wrapping21) {}
  ~RtcTestEvent() override = default;

  Type GetType() const override { return static_cast<Type>(4711); }
  bool IsConfigEvent() const override { return false; }

  static constexpr EventParameters event_params{
      "TestEvent", static_cast<RtcEvent::Type>(4711)};
  static constexpr FieldParameters timestamp_params{
      "timestamp_ms", FieldParameters::kTimestampField, FieldType::kVarInt, 64};
  static constexpr FieldParameters bool_params{"b", 2, FieldType::kFixed8, 1};
  static constexpr FieldParameters signed32_params{"signed32", 3,
                                                   FieldType::kVarInt, 32};
  static constexpr FieldParameters unsigned32_params{"unsigned32", 4,
                                                     FieldType::kFixed32, 32};
  static constexpr FieldParameters signed64_params{"signed64", 5,
                                                   FieldType::kFixed64, 64};
  static constexpr FieldParameters unsigned64_params{"unsigned64", 6,
                                                     FieldType::kVarInt, 64};
  static constexpr FieldParameters optional32_params{"optional_signed32", 7,
                                                     FieldType::kFixed32, 32};
  static constexpr FieldParameters optional64_params{"optional_signed64", 8,
                                                     FieldType::kVarInt, 64};
  static constexpr FieldParameters wrapping21_params{"wrapping21", 9,
                                                     FieldType::kFixed32, 21};

  static constexpr Type kType = static_cast<RtcEvent::Type>(4711);

  const bool b_;
  const int32_t signed32_;
  const uint32_t unsigned32_;
  const int64_t signed64_;
  const uint64_t unsigned64_;
  const absl::optional<int32_t> optional_signed32_ = absl::nullopt;
  const absl::optional<int64_t> optional_signed64_ = absl::nullopt;
  const uint32_t wrapping21_ = 0;
};

constexpr EventParameters RtcTestEvent::event_params;
constexpr FieldParameters RtcTestEvent::timestamp_params;
constexpr FieldParameters RtcTestEvent::bool_params;
constexpr FieldParameters RtcTestEvent::signed32_params;
constexpr FieldParameters RtcTestEvent::unsigned32_params;
constexpr FieldParameters RtcTestEvent::signed64_params;
constexpr FieldParameters RtcTestEvent::unsigned64_params;

constexpr FieldParameters RtcTestEvent::optional32_params;
constexpr FieldParameters RtcTestEvent::optional64_params;
constexpr FieldParameters RtcTestEvent::wrapping21_params;

constexpr RtcEvent::Type RtcTestEvent::kType;

class RtcEventFieldTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void CreateFullEvents(
      const std::vector<bool>& bool_values,
      const std::vector<int32_t>& signed32_values,
      const std::vector<uint32_t>& unsigned32_values,
      const std::vector<int64_t>& signed64_values,
      const std::vector<uint64_t>& unsigned64_values,
      const std::vector<absl::optional<int32_t>>& optional32_values,
      const std::vector<absl::optional<int64_t>>& optional64_values,
      const std::vector<uint32_t>& wrapping21_values) {
    size_t size = bool_values.size();
    RTC_CHECK_EQ(signed32_values.size(), size);
    RTC_CHECK_EQ(unsigned32_values.size(), size);
    RTC_CHECK_EQ(signed64_values.size(), size);
    RTC_CHECK_EQ(unsigned64_values.size(), size);
    RTC_CHECK_EQ(optional32_values.size(), size);
    RTC_CHECK_EQ(optional64_values.size(), size);
    RTC_CHECK_EQ(wrapping21_values.size(), size);

    for (size_t i = 0; i < size; i++) {
      batch_.push_back(new RtcTestEvent(
          bool_values[i], signed32_values[i], unsigned32_values[i],
          signed64_values[i], unsigned64_values[i], optional32_values[i],
          optional64_values[i], wrapping21_values[i]));
    }
  }

  void PrintBytes(const std::string& s) {
    for (auto c : s) {
      fprintf(stderr, "%d ", static_cast<uint8_t>(c));
    }
    fprintf(stderr, "\n");
  }

  void ParseEventHeader(absl::string_view encoded_event) {
    uint64_t event_tag;
    bool success;
    std::tie(success, encoded_event) = DecodeVarInt(encoded_event, &event_tag);
    ASSERT_TRUE(success);
    uint64_t event_id = event_tag >> 1;
    ASSERT_EQ(event_id, static_cast<uint64_t>(RtcTestEvent::event_params.id));
    bool batched = event_tag & 1u;
    ASSERT_EQ(batched, batch_.size() > 1u);

    uint64_t size;
    std::tie(success, encoded_event) = DecodeVarInt(encoded_event, &size);
    ASSERT_EQ(encoded_event.size(), size);

    ASSERT_TRUE(parser_.Initialize(encoded_event, batched).ok());
  }

  void ParseAndVerifyTimestamps() {
    std::vector<uint64_t> values;
    auto status = parser_.ParseField(RtcTestEvent::timestamp_params, &values);
    ASSERT_TRUE(status.ok()) << status.message().c_str();
    ASSERT_EQ(values.size(), batch_.size());
    for (size_t i = 0; i < batch_.size(); i++) {
      EXPECT_EQ(values[i], static_cast<uint64_t>(batch_[i]->timestamp_ms()));
    }
  }

  template <typename T>
  void ParseAndVerifyField(const FieldParameters& params,
                           const std::vector<T>& expected_values,
                           size_t expected_size) {
    std::vector<uint64_t> values;
    size_t size_before = parser_.remaining_bytes();
    auto status = parser_.ParseField(params, &values);
    ASSERT_TRUE(status.ok()) << status.message().c_str();
    ASSERT_EQ(values.size(), expected_values.size());
    for (size_t i = 0; i < expected_values.size(); i++) {
      EXPECT_EQ(ConvertToSignedIfSignedType<T>(values[i]), expected_values[i]);
    }
    size_t size_after = parser_.remaining_bytes();
    EXPECT_EQ(size_before - size_after, expected_size)
        << " for field " << params.name;
  }

  template <typename T>
  void ParseAndVerifyOptionalField(
      const FieldParameters& params,
      const std::vector<absl::optional<T>>& expected_values,
      size_t expected_size) {
    std::vector<bool> positions;
    positions.reserve(expected_values.size());
    std::vector<uint64_t> values;
    values.reserve(expected_values.size());
    size_t size_before = parser_.remaining_bytes();
    auto status = parser_.ParseField(params, &positions, &values);
    ASSERT_TRUE(status.ok()) << status.message().c_str();
    auto value_it = values.begin();
    ASSERT_EQ(positions.size(), expected_values.size());
    for (size_t i = 0; i < expected_values.size(); i++) {
      if (positions[i]) {
        ASSERT_NE(value_it, values.end());
        ASSERT_TRUE(expected_values[i].has_value());
        EXPECT_EQ(ConvertToSignedIfSignedType<T>(*value_it),
                  expected_values[i].value());
        ++value_it;
      } else {
        EXPECT_EQ(absl::nullopt, expected_values[i]);
      }
    }
    EXPECT_EQ(value_it, values.end());
    size_t size_after = parser_.remaining_bytes();
    EXPECT_EQ(size_before - size_after, expected_size);
  }

  void ParseAndVerifyMissingField(const FieldParameters& params) {
    std::vector<uint64_t> values{4711};
    auto status = parser_.ParseField(params, &values);
    ASSERT_TRUE(status.ok()) << status.message().c_str();
    EXPECT_EQ(values.size(), 0u);
  }

  void ParseAndVerifyMissingOptionalField(const FieldParameters& params) {
    std::vector<bool> positions{true, false};
    std::vector<uint64_t> values{4711};
    auto status = parser_.ParseField(params, &positions, &values);
    ASSERT_TRUE(status.ok()) << status.message().c_str();
    EXPECT_EQ(positions.size(), 0u);
    EXPECT_EQ(values.size(), 0u);
  }

  void TearDown() override {
    for (const RtcEvent* event : batch_) {
      delete event;
    }
  }

  std::vector<const RtcEvent*> batch_;
  EventParser parser_;
};

TEST_F(RtcEventFieldTest, EmptyList) {
  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::bool_params,
                      Extract(batch_, &RtcTestEvent::b_));
  std::string s = encoder.AsString();
  EXPECT_TRUE(s.empty());
}

TEST_F(RtcEventFieldTest, Singleton) {
  std::vector<bool> bool_values = {true};
  std::vector<int32_t> signed32_values = {-2};
  std::vector<uint32_t> unsigned32_values = {123456789};
  std::vector<int64_t> signed64_values = {-9876543210};
  std::vector<uint64_t> unsigned64_values = {9876543210};
  std::vector<absl::optional<int32_t>> optional32_values = {kInt32Min};
  std::vector<absl::optional<int64_t>> optional64_values = {kInt64Max};
  std::vector<uint32_t> wrapping21_values = {(1 << 21) - 1};

  size_t bool_encoding_size = /*tag*/ 1 + /* fixed8 base*/ 1;
  size_t signed32_encoding_size = /*tag*/ 1 + /* varint base*/ 5;
  size_t unsigned32_encoding_size = /*tag*/ 1 + /* fixed32 base*/ 4;
  size_t signed64_encoding_size = /*tag*/ 1 + /* fixed64 base*/ 8;
  size_t unsigned64_encoding_size = /*tag*/ 1 + /* varint base*/ 5;
  size_t optional32_encoding_size = /*tag*/ 1 + /* fixed32 base*/ 4;
  size_t optional64_encoding_size = /*tag*/ 1 + /* varint base*/ 9;
  size_t wrapping21_encoding_size = /*tag*/ 1 + /* fixed32 base*/ 4;

  CreateFullEvents(bool_values, signed32_values, unsigned32_values,
                   signed64_values, unsigned64_values, optional32_values,
                   optional64_values, wrapping21_values);

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::bool_params,
                      Extract(batch_, &RtcTestEvent::b_));
  encoder.EncodeField(RtcTestEvent::signed32_params,
                      Extract(batch_, &RtcTestEvent::signed32_));
  encoder.EncodeField(RtcTestEvent::unsigned32_params,
                      Extract(batch_, &RtcTestEvent::unsigned32_));
  encoder.EncodeField(RtcTestEvent::signed64_params,
                      Extract(batch_, &RtcTestEvent::signed64_));
  encoder.EncodeField(RtcTestEvent::unsigned64_params,
                      Extract(batch_, &RtcTestEvent::unsigned64_));
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug printing
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyField(RtcTestEvent::bool_params, bool_values,
                      bool_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed32_params, signed32_values,
                      signed32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned32_params, unsigned32_values,
                      unsigned32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed64_params, signed64_values,
                      signed64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned64_params, unsigned64_values,
                      unsigned64_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional32_params,
                              optional32_values, optional32_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional64_params,
                              optional64_values, optional64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, EqualElements) {
  std::vector<bool> bool_values = {true, true, true, true};
  std::vector<int32_t> signed32_values = {-2, -2, -2, -2};
  std::vector<uint32_t> unsigned32_values = {123456789, 123456789, 123456789,
                                             123456789};
  std::vector<int64_t> signed64_values = {-9876543210, -9876543210, -9876543210,
                                          -9876543210};
  std::vector<uint64_t> unsigned64_values = {9876543210, 9876543210, 9876543210,
                                             9876543210};
  std::vector<absl::optional<int32_t>> optional32_values = {
      kInt32Min, kInt32Min, kInt32Min, kInt32Min};
  std::vector<absl::optional<int64_t>> optional64_values = {
      kInt64Max, kInt64Max, kInt64Max, kInt64Max};
  std::vector<uint32_t> wrapping21_values = {(1 << 21) - 1, (1 << 21) - 1,
                                             (1 << 21) - 1, (1 << 21) - 1};

  size_t bool_encoding_size =
      /*tag*/ 1 + /* fixed8 base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t signed32_encoding_size =
      /*tag*/ 1 + /* varint base*/ 5 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t unsigned32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t signed64_encoding_size =
      /*tag*/ 1 + /* fixed64 base*/ 8 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t unsigned64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 5 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t optional32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t optional64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 9 + /* delta_header*/ 1 + /*deltas*/ 0;
  size_t wrapping21_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 0;

  CreateFullEvents(bool_values, signed32_values, unsigned32_values,
                   signed64_values, unsigned64_values, optional32_values,
                   optional64_values, wrapping21_values);
  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::bool_params,
                      Extract(batch_, &RtcTestEvent::b_));
  encoder.EncodeField(RtcTestEvent::signed32_params,
                      Extract(batch_, &RtcTestEvent::signed32_));
  encoder.EncodeField(RtcTestEvent::unsigned32_params,
                      Extract(batch_, &RtcTestEvent::unsigned32_));
  encoder.EncodeField(RtcTestEvent::signed64_params,
                      Extract(batch_, &RtcTestEvent::signed64_));
  encoder.EncodeField(RtcTestEvent::unsigned64_params,
                      Extract(batch_, &RtcTestEvent::unsigned64_));
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug printing
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyField(RtcTestEvent::bool_params, bool_values,
                      bool_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed32_params, signed32_values,
                      signed32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned32_params, unsigned32_values,
                      unsigned32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed64_params, signed64_values,
                      signed64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned64_params, unsigned64_values,
                      unsigned64_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional32_params,
                              optional32_values, optional32_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional64_params,
                              optional64_values, optional64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, Increasing) {
  std::vector<bool> bool_values = {false, true, false, true};
  std::vector<int32_t> signed32_values = {-2, -1, 0, 1};
  std::vector<uint32_t> unsigned32_values = {kUint32Max - 1, kUint32Max, 0, 1};
  std::vector<int64_t> signed64_values = {kInt64Max - 1, kInt64Max, kInt64Min,
                                          kInt64Min + 1};
  std::vector<uint64_t> unsigned64_values = {kUint64Max - 1, kUint64Max, 0, 1};
  std::vector<absl::optional<int32_t>> optional32_values = {
      kInt32Max - 1, kInt32Max, kInt32Min, kInt32Min + 1};
  std::vector<absl::optional<int64_t>> optional64_values = {
      kInt64Max - 1, kInt64Max, kInt64Min, kInt64Min + 1};
  std::vector<uint32_t> wrapping21_values = {(1 << 21) - 2, (1 << 21) - 1, 0,
                                             1};

  size_t bool_encoding_size =
      /*tag*/ 1 + /* fixed8 base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t signed32_encoding_size =
      /*tag*/ 1 + /* varint base*/ 5 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t unsigned32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t signed64_encoding_size =
      /*tag*/ 1 + /* fixed64 base*/ 8 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t unsigned64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 10 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t optional32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t optional64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 9 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t wrapping21_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 1;

  CreateFullEvents(bool_values, signed32_values, unsigned32_values,
                   signed64_values, unsigned64_values, optional32_values,
                   optional64_values, wrapping21_values);

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::bool_params,
                      Extract(batch_, &RtcTestEvent::b_));
  encoder.EncodeField(RtcTestEvent::signed32_params,
                      Extract(batch_, &RtcTestEvent::signed32_));
  encoder.EncodeField(RtcTestEvent::unsigned32_params,
                      Extract(batch_, &RtcTestEvent::unsigned32_));
  encoder.EncodeField(RtcTestEvent::signed64_params,
                      Extract(batch_, &RtcTestEvent::signed64_));
  encoder.EncodeField(RtcTestEvent::unsigned64_params,
                      Extract(batch_, &RtcTestEvent::unsigned64_));
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug printing
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyField(RtcTestEvent::bool_params, bool_values,
                      bool_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed32_params, signed32_values,
                      signed32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned32_params, unsigned32_values,
                      unsigned32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed64_params, signed64_values,
                      signed64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned64_params, unsigned64_values,
                      unsigned64_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional32_params,
                              optional32_values, optional32_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional64_params,
                              optional64_values, optional64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, Decreasing) {
  std::vector<bool> bool_values = {true, false, true, false};
  std::vector<int32_t> signed32_values = {2, 1, 0, -1};
  std::vector<uint32_t> unsigned32_values = {1, 0, kUint32Max, kUint32Max - 1};
  std::vector<int64_t> signed64_values = {kInt64Min + 1, kInt64Min, kInt64Max,
                                          kInt64Max - 1};
  std::vector<uint64_t> unsigned64_values = {1, 0, kUint64Max, kUint64Max - 1};
  std::vector<absl::optional<int32_t>> optional32_values = {
      kInt32Min + 1, kInt32Min, kInt32Max, kInt32Max - 1};
  std::vector<absl::optional<int64_t>> optional64_values = {
      kInt64Min + 1, kInt64Min, kInt64Max, kInt64Max - 1};
  std::vector<uint32_t> wrapping21_values = {1, 0, (1 << 21) - 1,
                                             (1 << 21) - 2};

  size_t bool_encoding_size =
      /*tag*/ 1 + /* fixed8 base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t signed32_encoding_size =
      /*tag*/ 1 + /* varint base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t unsigned32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t signed64_encoding_size =
      /*tag*/ 1 + /* fixed64 base*/ 8 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t unsigned64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t optional32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t optional64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 10 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t wrapping21_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 1;

  CreateFullEvents(bool_values, signed32_values, unsigned32_values,
                   signed64_values, unsigned64_values, optional32_values,
                   optional64_values, wrapping21_values);

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::bool_params,
                      Extract(batch_, &RtcTestEvent::b_));
  encoder.EncodeField(RtcTestEvent::signed32_params,
                      Extract(batch_, &RtcTestEvent::signed32_));
  encoder.EncodeField(RtcTestEvent::unsigned32_params,
                      Extract(batch_, &RtcTestEvent::unsigned32_));
  encoder.EncodeField(RtcTestEvent::signed64_params,
                      Extract(batch_, &RtcTestEvent::signed64_));
  encoder.EncodeField(RtcTestEvent::unsigned64_params,
                      Extract(batch_, &RtcTestEvent::unsigned64_));
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug printing
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyField(RtcTestEvent::bool_params, bool_values,
                      bool_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed32_params, signed32_values,
                      signed32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned32_params, unsigned32_values,
                      unsigned32_encoding_size);
  ParseAndVerifyField(RtcTestEvent::signed64_params, signed64_values,
                      signed64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::unsigned64_params, unsigned64_values,
                      unsigned64_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional32_params,
                              optional32_values, optional32_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional64_params,
                              optional64_values, optional64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, SkipsDeprecatedFields) {
  // Expect parser to skip fields it doesn't recognize, but find subsequent
  // fields.
  std::vector<bool> bool_values = {true, false};
  std::vector<int32_t> signed32_values = {kInt32Min / 2, kInt32Max / 2};
  std::vector<uint32_t> unsigned32_values = {0, kUint32Max / 2};
  std::vector<int64_t> signed64_values = {kInt64Min / 2, kInt64Max / 2};
  std::vector<uint64_t> unsigned64_values = {0, kUint64Max / 2};
  std::vector<absl::optional<int32_t>> optional32_values = {kInt32Max / 2,
                                                            kInt32Min / 2};
  std::vector<absl::optional<int64_t>> optional64_values = {kInt64Min / 2,
                                                            kInt64Max / 2};
  std::vector<uint32_t> wrapping21_values = {0, 1 << 20};

  size_t bool_encoding_size =
      /*tag*/ 1 + /* fixed8 base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 1;
  size_t signed32_encoding_size =
      /*tag*/ 1 + /* varint base*/ 5 + /* delta_header*/ 1 + /*deltas*/ 4;
  size_t unsigned32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 4;
  size_t signed64_encoding_size =
      /*tag*/ 1 + /* fixed64 base*/ 8 + /* delta_header*/ 1 + /*deltas*/ 8;
  size_t unsigned64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 1 + /* delta_header*/ 1 + /*deltas*/ 8;
  size_t optional32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 4;
  size_t optional64_encoding_size =
      /*tag*/ 1 + /* varint base*/ 10 + /* delta_header*/ 1 + /*deltas*/ 8;
  size_t wrapping21_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 3;

  CreateFullEvents(bool_values, signed32_values, unsigned32_values,
                   signed64_values, unsigned64_values, optional32_values,
                   optional64_values, wrapping21_values);

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::bool_params,
                      Extract(batch_, &RtcTestEvent::b_));
  encoder.EncodeField(RtcTestEvent::signed32_params,
                      Extract(batch_, &RtcTestEvent::signed32_));
  encoder.EncodeField(RtcTestEvent::unsigned32_params,
                      Extract(batch_, &RtcTestEvent::unsigned32_));
  encoder.EncodeField(RtcTestEvent::signed64_params,
                      Extract(batch_, &RtcTestEvent::signed64_));
  encoder.EncodeField(RtcTestEvent::unsigned64_params,
                      Extract(batch_, &RtcTestEvent::unsigned64_));
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug printing
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyField(RtcTestEvent::bool_params, bool_values,
                      bool_encoding_size);
  // Skips parsing the `signed32_values`. The following unsigned fields should
  // still be found.
  ParseAndVerifyField(RtcTestEvent::unsigned32_params, unsigned32_values,
                      signed32_encoding_size + unsigned32_encoding_size);
  // Skips parsing the `signed64_values`. The following unsigned fields should
  // still be found.
  ParseAndVerifyField(RtcTestEvent::unsigned64_params, unsigned64_values,
                      signed64_encoding_size + unsigned64_encoding_size);
  // Skips parsing the `optional32_values`. The following unsigned fields should
  // still be found.
  ParseAndVerifyOptionalField(
      RtcTestEvent::optional64_params, optional64_values,
      optional32_encoding_size + optional64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, SkipsMissingFields) {
  // Expect parsing of missing field to succeed but return an empty list.

  std::vector<bool> bool_values = {true, false};
  std::vector<int32_t> signed32_values = {kInt32Min / 2, kInt32Max / 2};
  std::vector<uint32_t> unsigned32_values = {0, kUint32Max / 2};
  std::vector<int64_t> signed64_values = {kInt64Min / 2, kInt64Max / 2};
  std::vector<uint64_t> unsigned64_values = {0, kUint64Max / 2};
  std::vector<absl::optional<int32_t>> optional32_values = {kInt32Max / 2,
                                                            kInt32Min / 2};
  std::vector<absl::optional<int64_t>> optional64_values = {kInt64Min / 2,
                                                            kInt64Max / 2};
  std::vector<uint32_t> wrapping21_values = {0, 1 << 20};

  size_t signed32_encoding_size =
      /*tag*/ 1 + /* varint base*/ 5 + /* delta_header*/ 1 + /*deltas*/ 4;
  size_t signed64_encoding_size =
      /*tag*/ 1 + /* fixed64 base*/ 8 + /* delta_header*/ 1 + /*deltas*/ 8;
  size_t optional32_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 4;
  size_t wrapping21_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 3;

  CreateFullEvents(bool_values, signed32_values, unsigned32_values,
                   signed64_values, unsigned64_values, optional32_values,
                   optional64_values, wrapping21_values);

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  // Skip encoding the `bool_values`.
  encoder.EncodeField(RtcTestEvent::signed32_params,
                      Extract(batch_, &RtcTestEvent::signed32_));
  // Skip encoding the `unsigned32_values`.
  encoder.EncodeField(RtcTestEvent::signed64_params,
                      Extract(batch_, &RtcTestEvent::signed64_));
  // Skip encoding the `unsigned64_values`.
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  // Skip encoding the `optional64_values`.
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug printing
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyMissingField(RtcTestEvent::bool_params);
  ParseAndVerifyField(RtcTestEvent::signed32_params, signed32_values,
                      signed32_encoding_size);
  ParseAndVerifyMissingField(RtcTestEvent::unsigned32_params);
  ParseAndVerifyField(RtcTestEvent::signed64_params, signed64_values,
                      signed64_encoding_size);
  ParseAndVerifyMissingField(RtcTestEvent::unsigned64_params);
  ParseAndVerifyOptionalField(RtcTestEvent::optional32_params,
                              optional32_values, optional32_encoding_size);
  ParseAndVerifyMissingOptionalField(RtcTestEvent::optional64_params);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, OptionalFields) {
  std::vector<absl::optional<int32_t>> optional32_values = {
      2, absl::nullopt, 4, absl::nullopt, 6, absl::nullopt};
  std::vector<absl::optional<int64_t>> optional64_values = {
      absl::nullopt, 1024, absl::nullopt, 1025, absl::nullopt, 1026};
  std::vector<uint32_t> wrapping21_values = {(1 << 21) - 3, 0, 2, 5, 5, 6};

  size_t optional32_encoding_size = /*tag*/ 1 + /* fixed32 base*/ 4 +
                                    /* delta_header*/ 2 + /* positions */ 1 +
                                    /*deltas*/ 1;
  size_t optional64_encoding_size = /*tag*/ 1 + /* varint base*/ 2 +
                                    /* delta_header*/ 2 + /* positions */ 1 +
                                    /*deltas*/ 1;
  size_t wrapping21_encoding_size =
      /*tag*/ 1 + /* fixed32 base*/ 4 + /* delta_header*/ 1 + /*deltas*/ 2;

  for (size_t i = 0; i < optional32_values.size(); i++) {
    batch_.push_back(new RtcTestEvent(0, 0, 0, 0, 0, optional32_values[i],
                                      optional64_values[i],
                                      wrapping21_values[i]));
  }

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  encoder.EncodeField(RtcTestEvent::wrapping21_params,
                      Extract(batch_, &RtcTestEvent::wrapping21_));
  std::string s = encoder.AsString();

  // Optional debug output
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyOptionalField(RtcTestEvent::optional32_params,
                              optional32_values, optional32_encoding_size);
  ParseAndVerifyOptionalField(RtcTestEvent::optional64_params,
                              optional64_values, optional64_encoding_size);
  ParseAndVerifyField(RtcTestEvent::wrapping21_params, wrapping21_values,
                      wrapping21_encoding_size);
}

TEST_F(RtcEventFieldTest, AllNulloptTreatedAsMissing) {
  std::vector<absl::optional<int32_t>> optional32_values = {
      absl::nullopt, absl::nullopt, absl::nullopt,
      absl::nullopt, absl::nullopt, absl::nullopt};
  std::vector<absl::optional<int64_t>> optional64_values = {
      absl::nullopt, 1024, absl::nullopt, 1025, absl::nullopt, 1026};

  size_t optional64_encoding_size = /*tag*/ 1 + /* varint base*/ 2 +
                                    /* delta_header*/ 2 + /* positions */ 1 +
                                    /*deltas*/ 1;

  for (size_t i = 0; i < optional32_values.size(); i++) {
    batch_.push_back(new RtcTestEvent(0, 0, 0, 0, 0, optional32_values[i],
                                      optional64_values[i], 0));
  }

  EventEncoder encoder(RtcTestEvent::event_params, batch_);
  encoder.EncodeField(RtcTestEvent::optional32_params,
                      Extract(batch_, &RtcTestEvent::optional_signed32_));
  encoder.EncodeField(RtcTestEvent::optional64_params,
                      Extract(batch_, &RtcTestEvent::optional_signed64_));
  std::string s = encoder.AsString();

  // Optional debug output
  // PrintBytes(s);

  ParseEventHeader(s);
  ParseAndVerifyTimestamps();
  ParseAndVerifyMissingOptionalField(RtcTestEvent::optional32_params);
  ParseAndVerifyOptionalField(RtcTestEvent::optional64_params,
                              optional64_values, optional64_encoding_size);
}

}  // namespace webrtc
