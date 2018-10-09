/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/gtest.h"

using webrtc::RtpExtension;

namespace cricket {
namespace {

webrtc::RtpHeaderExtensions MakeUniqueExtensions() {
  webrtc::RtpHeaderExtensions result;
  char name[] = "a";
  for (int i = 0; i < 7; ++i) {
    result.push_back(RtpExtension(name, 1 + i));
    name[0]++;
    result.push_back(RtpExtension(name, 255 - i));
    name[0]++;
  }
  return result;
}

webrtc::RtpHeaderExtensions MakeRedundantExtensions() {
  webrtc::RtpHeaderExtensions result;
  char name[] = "a";
  for (int i = 0; i < 7; ++i) {
    result.push_back(RtpExtension(name, 1 + i));
    result.push_back(RtpExtension(name, 255 - i));
    name[0]++;
  }
  return result;
}

bool SupportedExtensions1(const std::string& name) {
  return name == "c" || name == "i";
}

bool SupportedExtensions2(const std::string& name) {
  return name != "a" && name != "n";
}

bool IsSorted(const webrtc::RtpHeaderExtensions& extensions) {
  const std::string* last = nullptr;
  for (const auto& extension : extensions) {
    if (last && *last > extension.uri) {
      return false;
    }
    last = &extension.uri;
  }
  return true;
}
}  // namespace

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_EmptyList) {
  webrtc::RtpHeaderExtensions extensions;
  EXPECT_TRUE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_AllGood) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  EXPECT_TRUE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OutOfRangeId_Low) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 0));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OutOfRangeId_High) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 256));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OverlappingIds_StartOfSet) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 1));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OverlappingIds_EndOfSet) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 255));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_EmptyList) {
  webrtc::RtpHeaderExtensions extensions;
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions1, true);
  EXPECT_EQ(0u, filtered.size());
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_IncludeOnlySupported) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions1, false);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ("c", filtered[0].uri);
  EXPECT_EQ("i", filtered[1].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_SortedByName_1) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, false);
  EXPECT_EQ(12u, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_SortedByName_2) {
  webrtc::RtpHeaderExtensions extensions = MakeUniqueExtensions();
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(12u, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_DontRemoveRedundant) {
  webrtc::RtpHeaderExtensions extensions = MakeRedundantExtensions();
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, false);
  EXPECT_EQ(12u, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_EQ(filtered[0].uri, filtered[1].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundant) {
  webrtc::RtpHeaderExtensions extensions = MakeRedundantExtensions();
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(6u, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_NE(filtered[0].uri, filtered[1].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantEncrypted_1) {
  webrtc::RtpHeaderExtensions extensions;
  extensions.push_back(webrtc::RtpExtension("b", 1));
  extensions.push_back(webrtc::RtpExtension("b", 2, true));
  extensions.push_back(webrtc::RtpExtension("c", 3));
  extensions.push_back(webrtc::RtpExtension("b", 4));
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(3u, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_EQ(filtered[0].uri, filtered[1].uri);
  EXPECT_NE(filtered[0].encrypt, filtered[1].encrypt);
  EXPECT_NE(filtered[0].uri, filtered[2].uri);
  EXPECT_NE(filtered[1].uri, filtered[2].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantEncrypted_2) {
  webrtc::RtpHeaderExtensions extensions;
  extensions.push_back(webrtc::RtpExtension("b", 1, true));
  extensions.push_back(webrtc::RtpExtension("b", 2));
  extensions.push_back(webrtc::RtpExtension("c", 3));
  extensions.push_back(webrtc::RtpExtension("b", 4));
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(3u, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_EQ(filtered[0].uri, filtered[1].uri);
  EXPECT_NE(filtered[0].encrypt, filtered[1].encrypt);
  EXPECT_NE(filtered[0].uri, filtered[2].uri);
  EXPECT_NE(filtered[1].uri, filtered[2].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_1) {
  webrtc::RtpHeaderExtensions extensions;
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 3));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 9));
  extensions.push_back(RtpExtension(RtpExtension::kAbsSendTimeUri, 6));
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 1));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 14));
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(RtpExtension::kTransportSequenceNumberUri, filtered[0].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBweEncrypted_1) {
  webrtc::RtpHeaderExtensions extensions;
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 3));
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 4, true));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 9));
  extensions.push_back(RtpExtension(RtpExtension::kAbsSendTimeUri, 6));
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 1));
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 2, true));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 14));
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ(RtpExtension::kTransportSequenceNumberUri, filtered[0].uri);
  EXPECT_EQ(RtpExtension::kTransportSequenceNumberUri, filtered[1].uri);
  EXPECT_NE(filtered[0].encrypt, filtered[1].encrypt);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_2) {
  webrtc::RtpHeaderExtensions extensions;
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 1));
  extensions.push_back(RtpExtension(RtpExtension::kAbsSendTimeUri, 14));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 7));
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(RtpExtension::kAbsSendTimeUri, filtered[0].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_3) {
  webrtc::RtpHeaderExtensions extensions;
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 2));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 14));
  webrtc::RtpHeaderExtensions filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(RtpExtension::kTimestampOffsetUri, filtered[0].uri);
}

TEST(WebRtcMediaEngineFactoryTest, CreateWithBuiltinDecoders) {
  std::unique_ptr<MediaEngineInterface> engine(WebRtcMediaEngineFactory::Create(
      nullptr /* adm */, webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
      webrtc::AudioProcessingBuilder().Create()));
  EXPECT_TRUE(engine);
}

}  // namespace cricket
