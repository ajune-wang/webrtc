#include "pc/audio_rtp_receiver.h"

#include "media/base/media_channel.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"

using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

namespace cricket {
class MockVoiceMediaChannel : public VoiceMediaChannel {
 public:

  MockVoiceMediaChannel(webrtc::TaskQueueBase* network_thread) : VoiceMediaChannel(network_thread) {}

   MOCK_METHOD(void,
              SetInterface,
              (NetworkInterface* iface),
              (override));
   MOCK_METHOD(void,
              OnPacketReceived,
              (rtc::CopyOnWriteBuffer packet,
                int64_t packet_time_us),
              (override));
   MOCK_METHOD(void,
              OnPacketSent,
              (const rtc::SentPacket& sent_packet),
              (override));
   MOCK_METHOD(void,
              OnReadyToSend,
              (bool ready),
              (override));
   MOCK_METHOD(void,
              OnNetworkRouteChanged,
              (const std::string& transport_name,
              const rtc::NetworkRoute& network_route),
              (override));
   MOCK_METHOD(bool,
              AddSendStream,
              (const StreamParams& sp),
              (override));
   MOCK_METHOD(bool,
              RemoveSendStream,
              (uint32_t ssrc),
              (override));
   MOCK_METHOD(bool,
              AddRecvStream,
              (const StreamParams& sp),
              (override));
   MOCK_METHOD(bool,
              RemoveRecvStream,
              (uint32_t ssrc),
              (override));
   MOCK_METHOD(void,
              ResetUnsignaledRecvStream,
              (),
              (override));
   MOCK_METHOD(void,
              OnDemuxerCriteriaUpdatePending,
              (),
              (override));
   MOCK_METHOD(void,
              OnDemuxerCriteriaUpdateComplete,
              (),
              (override));
   MOCK_METHOD(int,
              GetRtpSendTimeExtnId,
              (),
              (const override));
   MOCK_METHOD(void,
              SetFrameEncryptor,
              (uint32_t ssrc,
              rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor),
              (override));
   MOCK_METHOD(void,
              SetFrameDecryptor,
              (uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor),
              (override));
   MOCK_METHOD(void,
              SetVideoCodecSwitchingEnabled,
              (bool enabled),
              (override));
   MOCK_METHOD(webrtc::RtpParameters,
              GetRtpSendParameters,
              (uint32_t ssrc),
              (const override));
   MOCK_METHOD(webrtc::RTCError,
              SetRtpSendParameters,
              (uint32_t ssrc,
      const webrtc::RtpParameters& parameters),
              (override));
   MOCK_METHOD(void,
              SetEncoderToPacketizerFrameTransformer,
              (uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer),
              (override));
   MOCK_METHOD(void,
              SetDepacketizerToDecoderFrameTransformer,
              (uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer),
              (override));


   MOCK_METHOD(bool,
              SetSendParameters,
              (const AudioSendParameters& params),
              (override));
   MOCK_METHOD(bool,
              SetRecvParameters,
              (const AudioRecvParameters& params),
              (override));
   MOCK_METHOD(webrtc::RtpParameters,
              GetRtpReceiveParameters,
              (uint32_t ssrc),
              (const override));
   MOCK_METHOD(webrtc::RtpParameters,
              GetDefaultRtpReceiveParameters,
              (),
              (const override));
   MOCK_METHOD(void,
              SetPlayout,
              (bool playout),
              (override));
   MOCK_METHOD(void,
              SetSend,
              (bool send),
              (override));
   MOCK_METHOD(bool,
              SetAudioSend,
              (uint32_t ssrc,
              bool enable,
              const AudioOptions* options,
              AudioSource* source),
              (override));
   MOCK_METHOD(bool,
              SetOutputVolume,
              (uint32_t ssrc, double volume),
              (override));
   MOCK_METHOD(bool,
              SetDefaultOutputVolume,
              (double volume),
              (override));
   MOCK_METHOD(bool,
              CanInsertDtmf,
              (),
              (override));
   MOCK_METHOD(bool,
              InsertDtmf,
              (uint32_t ssrc, int event, int duration),
              (override));
   MOCK_METHOD(bool,
              GetStats,
              (VoiceMediaInfo* info,
                        bool get_and_clear_legacy_stats),
              (override));
   MOCK_METHOD(void,
              SetRawAudioSink,
              (uint32_t ssrc,
      std::unique_ptr<webrtc::AudioSinkInterface> sink),
              (override));
   MOCK_METHOD(void,
              SetDefaultRawAudioSink,
              (std::unique_ptr<webrtc::AudioSinkInterface> sink),
              (override));
   MOCK_METHOD(std::vector<webrtc::RtpSource>,
              GetSources,
              (uint32_t ssrc),
              (const override));


   MOCK_METHOD(bool,
              SetBaseMinimumPlayoutDelayMs,
              (uint32_t ssrc, int delay_ms),
              (override));
   MOCK_METHOD(absl::optional<int>,
              GetBaseMinimumPlayoutDelayMs,
              (uint32_t ssrc),
              (const override));
};
}

namespace webrtc {
class AudioRtpReceiverTest : public ::testing::Test {
  protected:
  AudioRtpReceiverTest() : worker_(rtc::Thread::Current()),
      receiver_(rtc::make_ref_counted<AudioRtpReceiver>(worker_, std::string(), std::vector<std::string>(), false)),
      media_channel_(rtc::Thread::Current()) {}
  ~AudioRtpReceiverTest() {
    receiver_->SetMediaChannel(nullptr);
    receiver_->Stop();
  }

  rtc::Thread* worker_;
  rtc::scoped_refptr<AudioRtpReceiver> receiver_;
  cricket::MockVoiceMediaChannel media_channel_;

  test::RunLoop run_loop;
};

TEST_F(AudioRtpReceiverTest, SetVolume) {
  RTC_LOG(LS_ERROR) << "Starting SetVolumeTest \n";
  const double kVolume = 3.7;
  const double kDefaultVolume = 1;
  const uint32_t kSsrc = 3;

  EXPECT_CALL(media_channel_, SetOutputVolume(kSsrc, kDefaultVolume));

  RTC_LOG(LS_ERROR) << "SetVolumeTest 1 \n";
  receiver_->track()->set_enabled(true);
  RTC_LOG(LS_ERROR) << "SetVolumeTest 2 \n";
  receiver_->SetMediaChannel(&media_channel_);
  RTC_LOG(LS_ERROR) << "SetVolumeTest 3 \n";
  receiver_->SetupMediaChannel(kSsrc);
  RTC_LOG(LS_ERROR) << "SetVolumeTest 4 \n";

  EXPECT_CALL(media_channel_, SetOutputVolume(kSsrc, kVolume))
      .WillOnce(InvokeWithoutArgs([&] { run_loop.Quit(); return true;}));
  RTC_LOG(LS_ERROR) << "SetVolumeTest 5 \n";

  receiver_->OnSetVolume(kVolume);
  RTC_LOG(LS_ERROR) << "SetVolumeTest gets to end \n";

  run_loop.Run();
}

TEST_F(AudioRtpReceiverTest, SetVolumeBeforeStarting) {
  const double kVolume = 3.7;
  const uint32_t kSsrc = 3;

  EXPECT_CALL(media_channel_, SetOutputVolume(kSsrc, kVolume));

  receiver_->OnSetVolume(kVolume);

  receiver_->track()->set_enabled(true);
  receiver_->SetMediaChannel(&media_channel_);
  receiver_->SetupMediaChannel(kSsrc);
}
} // namespace webrtc