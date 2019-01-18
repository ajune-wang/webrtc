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

#include <tuple>

#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

constexpr uint8_t kDeprecatedFlags = 0x30;

class RtpGenericFrameDescriptorExtensionTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  RtpGenericFrameDescriptorExtensionTest()
      : arbitrary_bool_(std::get<0>(GetParam())),
        discardability_flag_used_(std::get<1>(GetParam())),
        field_trials_(discardability_flag_used_
                          ? "WebRTC-DiscardabilityFlag/Enabled/"
                          : "WebRTC-DiscardabilityFlag/Disabled/") {}
  ~RtpGenericFrameDescriptorExtensionTest() override = default;

  const bool arbitrary_bool_;
  const bool discardability_flag_used_;
  const test::ScopedFieldTrials field_trials_;
};

INSTANTIATE_TEST_CASE_P(,
                        RtpGenericFrameDescriptorExtensionTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool()));

// TODO(danilchap): Add fuzzer to test for various invalid inputs.

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseFirstPacketOfIndependenSubFrame) {
  const int kTemporalLayer = 5;
  uint8_t raw[] = {0x80 | kTemporalLayer, 0x49, 0x12, 0x34};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));

  EXPECT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastPacketInSubFrame());
  if (discardability_flag_used_) {
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
  raw[0] |= discardability_flag_used_ ? 0x00 : kDeprecatedFlags;

  RtpGenericFrameDescriptor descriptor;

  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.SetTemporalLayer(kTemporalLayer);
  descriptor.SetSpatialLayersBitmask(0x49);
  descriptor.SetFrameId(0x3412);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseLastPacketOfSubFrame) {
  uint8_t raw[1];
  raw[0] = arbitrary_bool_ ? 0x40 : 0x00;

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));

  ASSERT_FALSE(descriptor.FirstPacketInSubFrame());
  if (discardability_flag_used_) {
    ASSERT_FALSE(descriptor.Discardable());
  } else {
    ASSERT_FALSE(descriptor.FirstSubFrameInFrame());
    ASSERT_FALSE(descriptor.LastSubFrameInFrame());
  }

  EXPECT_EQ(descriptor.LastPacketInSubFrame(), arbitrary_bool_);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteLastPacketOfSubFrame) {
  uint8_t raw[1];
  raw[0] = (arbitrary_bool_ ? 0x40 : 0x00);
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  descriptor.SetLastPacketInSubFrame(arbitrary_bool_);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseFirstSubFrameInFrame) {
  if (discardability_flag_used_) {
    return;  // The test is irrelevant for this input permutation.
  }

  uint8_t raw[1];
  raw[0] = arbitrary_bool_ ? 0x20 : 0x00;

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));

  ASSERT_FALSE(descriptor.FirstPacketInSubFrame());
  ASSERT_FALSE(descriptor.LastPacketInSubFrame());
  ASSERT_FALSE(descriptor.LastSubFrameInFrame());

  EXPECT_EQ(descriptor.FirstSubFrameInFrame(), arbitrary_bool_);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseLastSubFrameInFrame) {
  if (discardability_flag_used_) {
    return;  // The test is irrelevant for this input permutation.
  }

  uint8_t raw[1];
  raw[0] = arbitrary_bool_ ? 0x10 : 0x00;

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));

  ASSERT_FALSE(descriptor.FirstPacketInSubFrame());
  ASSERT_FALSE(descriptor.LastPacketInSubFrame());
  ASSERT_FALSE(descriptor.FirstSubFrameInFrame());

  EXPECT_EQ(descriptor.LastSubFrameInFrame(), arbitrary_bool_);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseDiscardabilityFlag) {
  if (!discardability_flag_used_) {
    return;  // The test is irrelevant for this input permutation.
  }

  uint8_t raw[1];
  raw[0] = arbitrary_bool_ ? 0x20 : 0x00;

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));

  ASSERT_FALSE(descriptor.FirstPacketInSubFrame());
  ASSERT_FALSE(descriptor.LastPacketInSubFrame());

  EXPECT_EQ(descriptor.Discardable(), arbitrary_bool_);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteDiscardabilityFlag) {
  if (!discardability_flag_used_) {
    return;  // The test is irrelevant for this input permutation.
  }

  uint8_t raw[1];
  raw[0] = arbitrary_bool_ ? 0x20 : 0x00;

  RtpGenericFrameDescriptor descriptor;

  descriptor.SetDiscardable(arbitrary_bool_);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x04};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x04};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfc};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfc};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x02, 0x01};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x02, 0x01};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfe, 0xff};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, 0xfe, 0xff};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, (kDiff1 << 2) | 0x01, kDiff2 << 2};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff1, kDiff2));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;

  uint8_t raw[] = {0x88, 0x01, 0x00, 0x00, (kDiff1 << 2) | 0x01, kDiff2 << 2};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff1);
  descriptor.AddFrameDependencyDiff(kDiff2);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;

  uint8_t raw[] = {0x80, 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(RtpGenericFrameDescriptorExtension::Parse(raw, &descriptor));
  EXPECT_EQ(descriptor.Width(), kWidth);
  EXPECT_EQ(descriptor.Height(), kHeight);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;

  uint8_t raw[] = {0x80, 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  raw[0] |= (discardability_flag_used_ ? 0x00 : kDeprecatedFlags);

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.SetResolution(kWidth, kHeight);

  ASSERT_EQ(RtpGenericFrameDescriptorExtension::ValueSize(descriptor),
            sizeof(raw));
  uint8_t buffer[sizeof(raw)];
  EXPECT_TRUE(RtpGenericFrameDescriptorExtension::Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(raw));
}

}  // namespace
}  // namespace webrtc
