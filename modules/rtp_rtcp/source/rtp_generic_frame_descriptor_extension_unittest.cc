/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

constexpr uint8_t kDeprecatedFlags = 0x30;

class RtpGenericFrameDescriptorExtensionTest
    : public ::testing::TestWithParam<bool> {
 public:
  RtpGenericFrameDescriptorExtensionTest()
      : use_discardability_flag_(GetParam()) {}
  ~RtpGenericFrameDescriptorExtensionTest() override = default;

  bool Parse(rtc::ArrayView<const uint8_t> data,
             RtpGenericFrameDescriptor* descriptor) {
    return RtpGenericFrameDescriptorExtension::Parse(
        data, use_discardability_flag_, descriptor);
  }

  size_t ValueSize(const RtpGenericFrameDescriptor& descriptor) {
    return RtpGenericFrameDescriptorExtension::ValueSize(
        use_discardability_flag_, descriptor);
  }

  bool Write(rtc::ArrayView<uint8_t> data,
             const RtpGenericFrameDescriptor& descriptor) {
    return RtpGenericFrameDescriptorExtension::Write(
        data, use_discardability_flag_, descriptor);
  }

  const bool use_discardability_flag_;
};

INSTANTIATE_TEST_CASE_P(,
                        RtpGenericFrameDescriptorExtensionTest,
                        ::testing::Bool());

// TODO(danilchap): Add fuzzer to test for various invalid inputs.

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseFirstPacketOfIndependenSubFrame) {
  const int kTemporalLayer = 5;
  uint8_t raw[] = {0x80 | kTemporalLayer, 0x49, 0x12, 0x34};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));

  EXPECT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastPacketInSubFrame());
  if (use_discardability_flag_) {
    EXPECT_FALSE(descriptor.Discardable());
  } else {
    EXPECT_TRUE(descriptor.FirstSubFrameInFrame());
    EXPECT_TRUE(descriptor.LastSubFrameInFrame());
  }
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), IsEmpty());
  EXPECT_EQ(descriptor.TemporalLayer(), kTemporalLayer);
  EXPECT_EQ(descriptor.SpatialLayersBitmask(), 0x49);
  EXPECT_EQ(descriptor.FrameId(), 0x3412);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteFirstPacketOfIndependenSubFrame) {
  const int kTemporalLayer = 5;
  uint8_t raw[] = {0x80 | kTemporalLayer, 0x49, 0x12, 0x34};
  raw[0] |= use_discardability_flag_ ? 0x00 : kDeprecatedFlags;

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.SetTemporalLayer(kTemporalLayer);
  descriptor.SetSpatialLayersBitmask(0x49);
  descriptor.SetFrameId(0x3412);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseLastPacketOfSubFrame) {
  uint8_t kRaw[] = {0x40};

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_FALSE(descriptor.FirstPacketInSubFrame());
  if (use_discardability_flag_) {
    EXPECT_FALSE(descriptor.Discardable());
  } else {
    EXPECT_FALSE(descriptor.FirstSubFrameInFrame());
    EXPECT_FALSE(descriptor.LastSubFrameInFrame());
  }

  EXPECT_TRUE(descriptor.LastPacketInSubFrame());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteLastPacketOfSubFrame) {
  uint8_t raw[1] = {0x40};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  descriptor.SetLastPacketInSubFrame(true);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseFirstSubFrameInFrame) {
  if (use_discardability_flag_) {
    // First/Last-SubFrame flags and discardability flag are mutually exclusive.
    return;
  }

  constexpr uint8_t kRaw[] = {0x20};

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_FALSE(descriptor.FirstPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastSubFrameInFrame());

  EXPECT_TRUE(descriptor.FirstSubFrameInFrame());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseLastSubFrameInFrame) {
  if (use_discardability_flag_) {
    // First/Last-SubFrame flags and discardability flag are mutually exclusive.
    return;
  }

  constexpr uint8_t kRaw[] = {0x10};

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_FALSE(descriptor.FirstPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastPacketInSubFrame());
  EXPECT_FALSE(descriptor.FirstSubFrameInFrame());

  EXPECT_TRUE(descriptor.LastSubFrameInFrame());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseDiscardabilityFlag) {
  if (!use_discardability_flag_) {
    // First/Last-SubFrame flags and discardability flag are mutually exclusive.
    return;
  }

  constexpr uint8_t kRaw[] = {0x20};

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_FALSE(descriptor.FirstPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastPacketInSubFrame());

  EXPECT_TRUE(descriptor.Discardable());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteDiscardabilityFlag) {
  if (!use_discardability_flag_) {
    // First/Last-SubFrame flags and discardability flag are mutually exclusive.
    return;
  }

  constexpr uint8_t kRaw[] = {0x20};

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  descriptor.SetDiscardable(true);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x04};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x04};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfc};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfc};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x02, 0x01};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x02, 0x01};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfe, 0xff};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfe, 0xff};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, (kDiff1 << 2) | 0x01, kDiff2 << 2};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff1, kDiff2));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, (kDiff1 << 2) | 0x01, kDiff2 << 2};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff1);
  descriptor.AddFrameDependencyDiff(kDiff2);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;

  uint8_t raw[] = {0x80, 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);

  ASSERT_TRUE(Parse(raw, &descriptor));
  EXPECT_EQ(descriptor.Width(), kWidth);
  EXPECT_EQ(descriptor.Height(), kHeight);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;

  uint8_t raw[] = {0x80, 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  raw[0] |= (use_discardability_flag_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor(use_discardability_flag_);
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.SetResolution(kWidth, kHeight);

  ASSERT_EQ(ValueSize(descriptor), sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

}  // namespace
}  // namespace webrtc
