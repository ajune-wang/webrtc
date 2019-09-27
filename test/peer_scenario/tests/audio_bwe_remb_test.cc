/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/media_session.h"
#include "pc/session_description.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {
void PrintBitrates() {
  PeerScenario s;
  PeerScenarioClient::Config peer_config;
  //  peer_config.rtc_config.sdp_semantics = SdpSemantics::kPlanB;
  auto caller = s.CreateClient(PeerScenarioClient::Config());
  auto callee = s.CreateClient(PeerScenarioClient::Config());
  auto link_builder = s.net()->NodeBuilder().delay_ms(20).capacity_kbps(90);
  auto send = link_builder.Build();
  auto ret_node = link_builder.Build().node;
  s.net()->CreateRoute(caller->endpoint(), {send.node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());
  auto signaling = s.ConnectSignaling(caller, callee, {send.node}, {ret_node});
  caller->CreateAudio("AUDIO", {});
  auto video_config = PeerScenarioClient::VideoSendTrackConfig();
  video_config.generator.squares_video->width = 640;
  video_config.generator.squares_video->height = 480;
  caller->CreateVideo("VIDEO", video_config);
  signaling.StartIceSignaling();
  rtc::Event offer_exchange_done;
  signaling.NegotiateSdp(
      [](SessionDescriptionInterface* offer) {
        for (auto& content : offer->description()->contents()) {
          auto desc = content.media_description();
          auto exts = desc->rtp_header_extensions();
          desc->ClearRtpHeaderExtensions();
          for (const auto& ext : exts) {
            if (ext.uri == ext.kTransportSequenceNumberUri ||
                ext.uri == ext.kTransportSequenceNumberV2Uri)
              continue;
            desc->AddRtpHeaderExtension(ext);
          }
        }
        {
          auto* audio_desc =
              cricket::GetFirstAudioContentDescription(offer->description());
          if (audio_desc) {
            for (auto codec : audio_desc->codecs()) {
              codec.RemoveFeedbackParam("transport-cc");
              audio_desc->AddOrReplaceCodec(codec);
            }
          }
        }
        auto video_desc =
            cricket::GetFirstVideoContentDescription(offer->description());
        for (auto codec : video_desc->codecs()) {
          codec.RemoveFeedbackParam("transport-cc");
          video_desc->AddOrReplaceCodec(codec);
        }
      },
      [&](const SessionDescriptionInterface& answer) {
        offer_exchange_done.Set();
      });
  RTC_CHECK(s.WaitAndProcess(&offer_exchange_done));
  class StatsPrinter : public rtc::RefCountedObject<StatsObserver> {
   public:
    void OnComplete(const StatsReports& reports) override {
      for (auto report : reports) {
        if (report->type() == StatsReport::StatsType::kStatsReportTypeBwe) {
          if (print_headers_) {
            for (auto value : report->values()) {
              if (value.first ==
                      StatsReport::kStatsValueNameAvailableReceiveBandwidth ||
                  value.first == StatsReport::kStatsValueNameBucketDelay ||
                  value.first == StatsReport::kStatsValueNameRetransmitBitrate)
                continue;
              printf("%25s,", value.second->display_name() + 4);
            }
            printf("\n");
            print_headers_ = false;
          }
          for (auto value : report->values()) {
            if (value.first ==
                    StatsReport::kStatsValueNameAvailableReceiveBandwidth ||
                value.first == StatsReport::kStatsValueNameBucketDelay ||
                value.first == StatsReport::kStatsValueNameRetransmitBitrate)
              continue;
            printf("%25s,", value.second->ToString().c_str());
          }
          printf("\n");
        }
      }
    }

   private:
    bool print_headers_ = true;
  };
  rtc::scoped_refptr<StatsPrinter> printer = new StatsPrinter();
  for (int i = 0; i < 3; i++) {
    s.ProcessMessages(TimeDelta::seconds(1));
    caller->pc()->GetStats(printer, nullptr,
                           PeerConnectionInterface::kStatsOutputLevelStandard);
  }
  printf("\n");
  send.config.link_capacity_kbps = 1000;
  send.simulation->SetConfig(send.config);
  for (int i = 0; i < 20; i++) {
    s.ProcessMessages(TimeDelta::seconds(4));
    caller->pc()->GetStats(printer, nullptr,
                           PeerConnectionInterface::kStatsOutputLevelStandard);
  }
}

TEST(AudioSendSideBweRembTest, WithoutTrial) {
  PrintBitrates();
}
TEST(AudioSendSideBweRembTest, WithTrial) {
  ScopedFieldTrials trials("WebRTC-Audio-SendSideBwe/Enabled/");
  PrintBitrates();
}
TEST(AudioSendSideBweRembTest, WithOverhead) {
  ScopedFieldTrials trials("WebRTC-SendSideBwe-WithOverhead/Enabled/");
  PrintBitrates();
}
TEST(AudioSendSideBweRembTest, WithBoth) {
  ScopedFieldTrials trials(
      "WebRTC-Audio-SendSideBwe/Enabled/"
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  PrintBitrates();
}

}  // namespace test
}  // namespace webrtc
