/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/apm_helpers.h"

#include "api/audio_options.h"
#include "api/scoped_refptr.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

struct TestHelper {
  TestHelper() {
    // This replicates the conditions from voe_auto_test.
    Config config;
    config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
    apm_ = rtc::scoped_refptr<AudioProcessing>(
        AudioProcessingBuilder().Create(config));
    apm_helpers::Init(apm());
  }

  AudioProcessing* apm() { return apm_.get(); }

  const AudioProcessing* apm() const { return apm_.get(); }

 private:
  rtc::scoped_refptr<AudioProcessing> apm_;
};
}  // namespace

TEST(ApmHelpersTest, Agc_DefaultConfiguration) {
  TestHelper helper;
  using AgcConfig = AudioProcessing::Config::GainController1;
  AgcConfig agc_config = helper.apm()->GetConfig().gain_controller1;
  cricket::AudioOptions options;
  apm_helpers::UpdateAgcConfig(options, &agc_config);

  EXPECT_FALSE(agc_config.enabled);
#if defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
  EXPECT_EQ(AgcConfig::kAdaptiveAnalog, agc_config.mode);
#elif defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  EXPECT_EQ(AgcConfig::kFixedDigital, agc_config.mode);
#else
  EXPECT_EQ(AgcConfig::kAdaptiveAnalog, agc_config.mode);
#endif
  EXPECT_EQ(3, agc_config.target_level_dbfs);
  EXPECT_EQ(9, agc_config.compression_gain_db);
  EXPECT_TRUE(agc_config.enable_limiter);
}

TEST(ApmHelpersTest, UpdateAgcConfig_NoOptionsNoChange) {
  AudioProcessing::Config::GainController1 kDefaultAgcConfig;
  AudioProcessing::Config::GainController1 agc_config;
  cricket::AudioOptions options;
  apm_helpers::UpdateAgcConfig(options, &agc_config);

  EXPECT_EQ(kDefaultAgcConfig.enabled, agc_config.enabled);
  EXPECT_EQ(kDefaultAgcConfig.mode, agc_config.mode);
  EXPECT_EQ(kDefaultAgcConfig.target_level_dbfs, agc_config.target_level_dbfs);
  EXPECT_EQ(kDefaultAgcConfig.compression_gain_db,
            agc_config.compression_gain_db);
  EXPECT_EQ(kDefaultAgcConfig.enable_limiter, agc_config.enable_limiter);
}

TEST(ApmHelpersTest, UpdateAgcConfig_SetAndForgetOptions) {
  AudioProcessing::Config::GainController1 agc_config;
  cricket::AudioOptions options;

  options.auto_gain_control = true;
  apm_helpers::UpdateAgcConfig(options, &agc_config);
  EXPECT_TRUE(agc_config.enabled);

  options.tx_agc_target_dbov = 5;
  apm_helpers::UpdateAgcConfig(options, &agc_config);
  EXPECT_EQ(5, agc_config.target_level_dbfs);
  options.tx_agc_target_dbov = absl::nullopt;

  options.tx_agc_digital_compression_gain = 10;
  apm_helpers::UpdateAgcConfig(options, &agc_config);
  EXPECT_EQ(10, agc_config.compression_gain_db);
  options.tx_agc_digital_compression_gain = absl::nullopt;

  options.tx_agc_limiter = false;
  apm_helpers::UpdateAgcConfig(options, &agc_config);
  EXPECT_FALSE(agc_config.enable_limiter);
  options.tx_agc_limiter = absl::nullopt;

  apm_helpers::UpdateAgcConfig(options, &agc_config);
  // Expect all options to have been preserved.
  EXPECT_TRUE(agc_config.enabled);
  EXPECT_EQ(5, agc_config.target_level_dbfs);
  EXPECT_EQ(10, agc_config.compression_gain_db);
  EXPECT_FALSE(agc_config.enable_limiter);
}

TEST(ApmHelpersTest, EcStatus_DefaultMode) {
  TestHelper helper;
  webrtc::AudioProcessing::Config config = helper.apm()->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);
}

TEST(ApmHelpersTest, EcStatus_EnableDisable) {
  TestHelper helper;
  webrtc::AudioProcessing::Config config;

  apm_helpers::SetEcStatus(helper.apm(), true, kEcAecm);
  config = helper.apm()->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_TRUE(config.echo_canceller.mobile_mode);

  apm_helpers::SetEcStatus(helper.apm(), false, kEcAecm);
  config = helper.apm()->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);

  apm_helpers::SetEcStatus(helper.apm(), true, kEcConference);
  config = helper.apm()->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_FALSE(config.echo_canceller.mobile_mode);

  apm_helpers::SetEcStatus(helper.apm(), false, kEcConference);
  config = helper.apm()->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);

  apm_helpers::SetEcStatus(helper.apm(), true, kEcAecm);
  config = helper.apm()->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_TRUE(config.echo_canceller.mobile_mode);
}

TEST(ApmHelpersTest, NsStatus_DefaultMode) {
  TestHelper helper;
  NoiseSuppression* ns = helper.apm()->noise_suppression();
  EXPECT_EQ(NoiseSuppression::kModerate, ns->level());
  EXPECT_FALSE(ns->is_enabled());
}

TEST(ApmHelpersTest, NsStatus_EnableDisable) {
  TestHelper helper;
  NoiseSuppression* ns = helper.apm()->noise_suppression();
  apm_helpers::SetNsStatus(helper.apm(), true);
  EXPECT_EQ(NoiseSuppression::kHigh, ns->level());
  EXPECT_TRUE(ns->is_enabled());
  apm_helpers::SetNsStatus(helper.apm(), false);
  EXPECT_EQ(NoiseSuppression::kHigh, ns->level());
  EXPECT_FALSE(ns->is_enabled());
}
}  // namespace webrtc
