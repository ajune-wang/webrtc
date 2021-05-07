/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/absolute_capture_time_receiver.h"

#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using AbsoluteCaptureTimeFlavors =
    AbsoluteCaptureTimeReceiver::AbsoluteCaptureTimeFlavors;
using ::testing::Eq;

TEST(AbsoluteCaptureTimeReceiverTest, GetSourceWithoutCsrcs) {
  constexpr uint32_t kSsrc = 12;

  EXPECT_EQ(AbsoluteCaptureTimeReceiver::GetSource(kSsrc, nullptr), kSsrc);
}

TEST(AbsoluteCaptureTimeReceiverTest, GetSourceWithCsrcs) {
  constexpr uint32_t kSsrc = 12;
  constexpr uint32_t kCsrcs[] = {34, 56, 78, 90};

  EXPECT_EQ(AbsoluteCaptureTimeReceiver::GetSource(kSsrc, kCsrcs), kCsrcs[0]);
}

// Checks that the `estimated_capture_clock_offset` property is available for
// the unadjusted extension flavor and unavailable for the adjusted one when the
// remote to local clock offset is not specified.
TEST(AbsoluteCaptureTimeReceiverTest,
     AdjustedUnavailableAndUnadjustedAvailable) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp = 1020300000;
  static const absl::optional<AbsoluteCaptureTime> kExtension =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  AbsoluteCaptureTimeFlavors flavor = receiver.OnReceivePacket(
      kSource, kRtpTimestamp, kRtpClockFrequency, kExtension);
  ASSERT_TRUE(flavor.unadjusted_clock_offset.has_value());
  EXPECT_TRUE(flavor.unadjusted_clock_offset->estimated_capture_clock_offset
                  .has_value());
  ASSERT_TRUE(flavor.adjusted_clock_offset.has_value());
  EXPECT_FALSE(
      flavor.adjusted_clock_offset->estimated_capture_clock_offset.has_value());
}

// Checks that the unadjusted and the adjusted flavors of the extension match
// when the remote to local clock offset is set to 0.
TEST(AbsoluteCaptureTimeReceiverTest, AdjustedAndUnadjustedClockOffsetMatch) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9020), absl::nullopt};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  AbsoluteCaptureTimeFlavors flavor0 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp0, kRtpClockFrequency, kExtension0);
  EXPECT_EQ(flavor0.unadjusted_clock_offset, flavor0.adjusted_clock_offset);

  AbsoluteCaptureTimeFlavors flavor1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_EQ(flavor1.unadjusted_clock_offset, flavor1.adjusted_clock_offset);
}

// Checks that the unadjusted and the adjusted flavors of the extension do not
// match when the remote to local clock offset is greater than 0 (if the
// `estimated_capture_clock_offset` is specified).
TEST(AbsoluteCaptureTimeReceiverTest,
     AdjustedAndUnadjustedClockOffsetDoNotMatch) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9020), absl::nullopt};
  static const absl::optional<int64_t> kRemoteToLocalClockOffset =
      Int64MsToQ32x32(-7000007);

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(kRemoteToLocalClockOffset);

  // `estimated_capture_clock_offset` specified.
  AbsoluteCaptureTimeFlavors flavor0 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp0, kRtpClockFrequency, kExtension0);
  EXPECT_NE(flavor0.unadjusted_clock_offset, flavor0.adjusted_clock_offset);

  // `estimated_capture_clock_offset` unspecified.
  AbsoluteCaptureTimeFlavors flavor1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_EQ(flavor1.unadjusted_clock_offset, flavor1.adjusted_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest, ReceiveExtensionReturnsExtension) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9020), absl::nullopt};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                               kExtension1),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension1,
                                    .adjusted_clock_offset = kExtension1}));
}

TEST(AbsoluteCaptureTimeReceiverTest, ReceiveNoExtensionReturnsNoExtension) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = absl::nullopt,
                                    .adjusted_clock_offset = absl::nullopt}));

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                               kExtension1),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = absl::nullopt,
                                    .adjusted_clock_offset = absl::nullopt}));
}

TEST(AbsoluteCaptureTimeReceiverTest, InterpolateLaterPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors1.unadjusted_clock_offset,
            extension_flavors1.adjusted_clock_offset);
  const auto& extension1 = extension_flavors1.adjusted_clock_offset;
  EXPECT_TRUE(extension1.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension1->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension1->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors2.unadjusted_clock_offset,
            extension_flavors2.adjusted_clock_offset);
  const auto& extension2 = extension_flavors2.adjusted_clock_offset;
  EXPECT_TRUE(extension2.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension2->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension2->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest, InterpolateEarlierPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors1.unadjusted_clock_offset,
            extension_flavors1.adjusted_clock_offset);
  const auto& extension1 = extension_flavors1.adjusted_clock_offset;
  EXPECT_TRUE(extension1.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension1->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension1->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors2.unadjusted_clock_offset,
            extension_flavors2.adjusted_clock_offset);
  const auto& extension2 = extension_flavors2.adjusted_clock_offset;
  EXPECT_TRUE(extension2.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension2->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 40);
  EXPECT_EQ(extension2->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     InterpolateLaterPacketArrivingLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = ~uint32_t{0} - 79;
  constexpr uint32_t kRtpTimestamp1 = 1280 - 80;
  constexpr uint32_t kRtpTimestamp2 = 2560 - 80;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors1.unadjusted_clock_offset,
            extension_flavors1.adjusted_clock_offset);
  const auto& extension1 = extension_flavors1.adjusted_clock_offset;
  EXPECT_TRUE(extension1.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension1->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension1->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors2.unadjusted_clock_offset,
            extension_flavors2.adjusted_clock_offset);
  const auto& extension2 = extension_flavors2.adjusted_clock_offset;
  EXPECT_TRUE(extension2.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension2->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension2->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     InterpolateEarlierPacketArrivingLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 799;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors1.unadjusted_clock_offset,
            extension_flavors1.adjusted_clock_offset);
  const auto& extension1 = extension_flavors1.adjusted_clock_offset;
  EXPECT_TRUE(extension1.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension1->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension1->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors2.unadjusted_clock_offset,
            extension_flavors2.adjusted_clock_offset);
  const auto& extension2 = extension_flavors2.adjusted_clock_offset;
  EXPECT_TRUE(extension2.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension2->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 40);
  EXPECT_EQ(extension2->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipEstimatedCaptureClockOffsetIfRemoteToLocalClockOffsetIsUnknown) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;
  static const absl::optional<int64_t> kRemoteToLocalClockOffset2 =
      Int64MsToQ32x32(-7000007);

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  receiver.SetRemoteToLocalClockOffset(absl::nullopt);

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_TRUE(extension_flavors1.adjusted_clock_offset.has_value());
  EXPECT_EQ(
      UQ32x32ToInt64Ms(
          extension_flavors1.adjusted_clock_offset->absolute_capture_timestamp),
      UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 20);
  EXPECT_TRUE(extension_flavors1.unadjusted_clock_offset.has_value());
  EXPECT_EQ(
      extension_flavors1.adjusted_clock_offset->absolute_capture_timestamp,
      extension_flavors1.unadjusted_clock_offset->absolute_capture_timestamp);
  // `estimated_capture_clock_offset` is unavailable for the adjusted extension
  // flavor, but it is available for the unadjusted one.
  EXPECT_EQ(
      extension_flavors1.adjusted_clock_offset->estimated_capture_clock_offset,
      absl::nullopt);
  EXPECT_EQ(extension_flavors1.unadjusted_clock_offset
                ->estimated_capture_clock_offset,
            *kExtension0->estimated_capture_clock_offset);

  receiver.SetRemoteToLocalClockOffset(kRemoteToLocalClockOffset2);

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  EXPECT_TRUE(extension_flavors2.adjusted_clock_offset.has_value());
  EXPECT_EQ(
      UQ32x32ToInt64Ms(
          extension_flavors2.adjusted_clock_offset->absolute_capture_timestamp),
      UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 40);
  EXPECT_TRUE(extension_flavors2.unadjusted_clock_offset.has_value());
  EXPECT_EQ(
      extension_flavors2.adjusted_clock_offset->absolute_capture_timestamp,
      extension_flavors2.unadjusted_clock_offset->absolute_capture_timestamp);
  // `estimated_capture_clock_offset` differs between the adjusted and the
  // unadjusted flavors.
  EXPECT_EQ(*extension_flavors2.adjusted_clock_offset
                    ->estimated_capture_clock_offset -
                *extension_flavors2.unadjusted_clock_offset
                     ->estimated_capture_clock_offset,
            *kRemoteToLocalClockOffset2);
}

TEST(AbsoluteCaptureTimeReceiverTest, SkipInterpolateIfTooLate) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  clock.AdvanceTime(AbsoluteCaptureTimeReceiver::kInterpolationMaxInterval);

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors1.unadjusted_clock_offset,
            extension_flavors1.adjusted_clock_offset);
  EXPECT_TRUE(extension_flavors1.adjusted_clock_offset.has_value());

  clock.AdvanceTimeMilliseconds(1);

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors2.unadjusted_clock_offset,
            extension_flavors2.adjusted_clock_offset);
  EXPECT_FALSE(extension_flavors2.adjusted_clock_offset.has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest, SkipInterpolateIfSourceChanged) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource0, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors = receiver.OnReceivePacket(
      kSource1, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors.unadjusted_clock_offset,
            extension_flavors.adjusted_clock_offset);
  EXPECT_FALSE(extension_flavors.adjusted_clock_offset.has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipInterpolateIfRtpClockFrequencyChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency0 = 64000;
  constexpr uint32_t kRtpClockFrequency1 = 32000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency0,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency1, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors.unadjusted_clock_offset,
            extension_flavors.adjusted_clock_offset);
  EXPECT_FALSE(extension_flavors.adjusted_clock_offset.has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipInterpolateIfRtpClockFrequencyIsInvalid) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 0;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors.unadjusted_clock_offset,
            extension_flavors.adjusted_clock_offset);
  EXPECT_FALSE(extension_flavors.adjusted_clock_offset.has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest, SkipInterpolateIsSticky) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr uint32_t kSource2 = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_THAT(
      receiver.OnReceivePacket(kSource0, kRtpTimestamp0, kRtpClockFrequency,
                               kExtension0),
      Eq(AbsoluteCaptureTimeFlavors{.unadjusted_clock_offset = kExtension0,
                                    .adjusted_clock_offset = kExtension0}));

  AbsoluteCaptureTimeFlavors extension_flavors1 = receiver.OnReceivePacket(
      kSource1, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors1.unadjusted_clock_offset,
            extension_flavors1.adjusted_clock_offset);
  EXPECT_FALSE(extension_flavors1.adjusted_clock_offset.has_value());

  AbsoluteCaptureTimeFlavors extension_flavors2 = receiver.OnReceivePacket(
      kSource2, kRtpTimestamp2, kRtpClockFrequency, kExtension2);
  // The remote to local clock offset is zero, so no need to check all the
  // flavors.
  ASSERT_EQ(extension_flavors2.unadjusted_clock_offset,
            extension_flavors2.adjusted_clock_offset);
  EXPECT_FALSE(extension_flavors2.adjusted_clock_offset.has_value());
}

}  // namespace
}  // namespace webrtc
