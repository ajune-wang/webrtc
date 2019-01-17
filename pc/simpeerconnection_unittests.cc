#if !defined(THREAD_SANITIZER)

#include <stdio.h>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "api/peerconnectionproxy.h"
#include "api/rtpreceiverinterface.h"
#include "api/umametrics.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "media/engine/fakewebrtcvideoengine.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/base/mockasyncresolver.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/portinterface.h"
#include "p2p/base/sim_core.h"
#include "p2p/base/teststunserver.h"
#include "p2p/base/testturncustomizer.h"
#include "p2p/base/testturnserver.h"
#include "p2p/client/basicportallocator.h"
#include "pc/dtmfsender.h"
#include "pc/localaudiosource.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/rtpmediautils.h"
#include "pc/sessiondescription.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakeperiodicvideotracksource.h"
#include "pc/test/fakertccertificategenerator.h"
#include "pc/test/fakevideotrackrenderer.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/firewallsocketserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/testcertificateverifier.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"

using cricket::ContentInfo;
using cricket::FakeWebRtcVideoDecoder;
using cricket::FakeWebRtcVideoDecoderFactory;
using cricket::FakeWebRtcVideoEncoder;
using cricket::FakeWebRtcVideoEncoderFactory;
using cricket::MediaContentDescription;
using cricket::StreamParams;
using rtc::SocketAddress;
using ::testing::_;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Values;
using webrtc::DataBuffer;
using webrtc::DataChannelInterface;
using webrtc::DtmfSender;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::FakeVideoTrackRenderer;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::MockCreateSessionDescriptionObserver;
using webrtc::MockDataChannelObserver;
using webrtc::MockSetSessionDescriptionObserver;
using webrtc::MockStatsObserver;
using webrtc::ObserverInterface;
using webrtc::PeerConnection;
using webrtc::PeerConnectionInterface;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using webrtc::PeerConnectionFactory;
using webrtc::PeerConnectionProxy;
using webrtc::RTCErrorType;
using webrtc::RTCTransportStats;
using webrtc::RtpReceiverInterface;
using webrtc::RtpSenderInterface;
using webrtc::RtpTransceiverDirection;
using webrtc::RtpTransceiverInit;
using webrtc::RtpTransceiverInterface;
using webrtc::SdpSemantics;
using webrtc::SdpType;
using webrtc::SessionDescriptionInterface;
using webrtc::SimConfig;
using webrtc::SimCore;
using webrtc::SimNetworkManager;
using webrtc::StreamCollectionInterface;
using webrtc::VideoTrackInterface;

namespace {

const webrtc::SimInterfaceConfig iface_config1{
    "tun1", "10.0.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_CELLULAR,
    webrtc::SimInterface::State::kUp};
const webrtc::SimInterfaceConfig iface_config2{
    "tun2", "172.16.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_WIFI,
    webrtc::SimInterface::State::kDown};
const webrtc::SimInterfaceConfig iface_config3{
    "tun3", "192.168.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_WIFI,
    webrtc::SimInterface::State::kUp};

const webrtc::SimLinkConfig link_config1{"bp2p_link1",
                                         webrtc::SimLink::Type::kPointToPoint,
                                         {"10.0.0.1", "192.168.0.1"},
                                         webrtc::SimLinkConfig::Params()};

static const int kDefaultTimeout = 10000;
static const int kMaxWaitForFramesMs = 10000;
// Default number of audio/video frames to wait for before considering a test
// successful.
static const int kDefaultExpectedAudioFrameCount = 3;
static const int kDefaultExpectedVideoFrameCount = 3;

static const SocketAddress kDefaultLocalAddress("192.168.1.1", 0);

class SignalingMessageReceiver {
 public:
  virtual void ReceiveSdpMessage(SdpType type, const std::string& msg) = 0;
  virtual void ReceiveIceMessage(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& msg) = 0;

 protected:
  SignalingMessageReceiver() {}
  virtual ~SignalingMessageReceiver() {}
};

class MockRtpReceiverObserver : public webrtc::RtpReceiverObserverInterface {
 public:
  explicit MockRtpReceiverObserver(cricket::MediaType media_type)
      : expected_media_type_(media_type) {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override {
    ASSERT_EQ(expected_media_type_, media_type);
    first_packet_received_ = true;
  }

  bool first_packet_received() const { return first_packet_received_; }

  virtual ~MockRtpReceiverObserver() {}

 private:
  bool first_packet_received_ = false;
  cricket::MediaType expected_media_type_;
};

class SimPeerConnectionWrapper : public webrtc::PeerConnectionObserver,
                                 public SignalingMessageReceiver {
 public:
  webrtc::PeerConnectionFactoryInterface* pc_factory() const {
    return peer_connection_factory_.get();
  }

  webrtc::PeerConnectionInterface* pc() const { return peer_connection_.get(); }

  // If a signaling message receiver is set (via ConnectFakeSignaling), this
  // will set the whole offer/answer exchange in motion. Just need to wait for
  // the signaling state to reach "stable".
  void CreateAndSetAndSignalOffer() {
    auto offer = CreateOffer();
    ASSERT_NE(nullptr, offer);
    EXPECT_TRUE(SetLocalDescriptionAndSendSdpMessage(std::move(offer)));
  }

  // Sets the options to be used when CreateAndSetAndSignalOffer is called, or
  // when a remote offer is received (via fake signaling) and an answer is
  // generated. By default, uses default options.
  void SetOfferAnswerOptions(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
    offer_answer_options_ = options;
  }

  // Set a callback to be invoked when a remote offer is received via the fake
  // signaling channel. This provides an opportunity to change the
  // PeerConnection state before an answer is created and sent to the caller.
  void SetRemoteOfferHandler(std::function<void()> handler) {
    remote_offer_handler_ = std::move(handler);
  }

  // Every ICE connection state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceConnectionState>
  ice_connection_state_history() const {
    return ice_connection_state_history_;
  }
  void clear_ice_connection_state_history() {
    ice_connection_state_history_.clear();
  }

  // Every ICE gathering state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceGatheringState>
  ice_gathering_state_history() const {
    return ice_gathering_state_history_;
  }

  void AddAudioVideoTracks() {
    AddAudioTrack();
    AddVideoTrack();
  }

  rtc::scoped_refptr<RtpSenderInterface> AddAudioTrack() {
    return AddTrack(CreateLocalAudioTrack());
  }

  rtc::scoped_refptr<RtpSenderInterface> AddVideoTrack() {
    return AddTrack(CreateLocalVideoTrack());
  }

  rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateLocalAudioTrack() {
    cricket::AudioOptions options;
    // Disable highpass filter so that we can get all the test audio frames.
    options.highpass_filter = false;
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(options);
    // TODO(perkj): Test audio source when it is implemented. Currently audio
    // always use the default input.
    return peer_connection_factory_->CreateAudioTrack(rtc::CreateRandomUuid(),
                                                      source);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack() {
    webrtc::FakePeriodicVideoSource::Config config;
    config.timestamp_offset_ms = rtc::TimeMillis();
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackWithConfig(
      webrtc::FakePeriodicVideoSource::Config config) {
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackWithRotation(webrtc::VideoRotation rotation) {
    webrtc::FakePeriodicVideoSource::Config config;
    config.rotation = rotation;
    config.timestamp_offset_ms = rtc::TimeMillis();
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids = {}) {
    auto result = pc()->AddTrack(track, stream_ids);
    EXPECT_EQ(RTCErrorType::NONE, result.error().type());
    return result.MoveValue();
  }

  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceiversOfType(
      cricket::MediaType media_type) {
    std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers;
    for (auto receiver : pc()->GetReceivers()) {
      if (receiver->media_type() == media_type) {
        receivers.push_back(receiver);
      }
    }
    return receivers;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> GetFirstTransceiverOfType(
      cricket::MediaType media_type) {
    for (auto transceiver : pc()->GetTransceivers()) {
      if (transceiver->receiver()->media_type() == media_type) {
        return transceiver;
      }
    }
    return nullptr;
  }

  bool SignalingStateStable() {
    return pc()->signaling_state() == webrtc::PeerConnectionInterface::kStable;
  }

  int audio_frames_received() const {
    return fake_audio_capture_module_->frames_received();
  }

  // Takes minimum of video frames received for each track.
  //
  // Can be used like:
  // EXPECT_GE(expected_frames, min_video_frames_received_per_track());
  //
  // To ensure that all video tracks received at least a certain number of
  // frames.
  int min_video_frames_received_per_track() const {
    int min_frames = INT_MAX;
    if (fake_video_renderers_.empty()) {
      return 0;
    }

    for (const auto& pair : fake_video_renderers_) {
      min_frames = std::min(min_frames, pair.second->num_rendered_frames());
    }
    return min_frames;
  }

  webrtc::PeerConnectionInterface::SignalingState signaling_state() {
    return pc()->signaling_state();
  }

  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state() {
    return pc()->ice_connection_state();
  }

  webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state() {
    return pc()->ice_gathering_state();
  }

  // Returns a MockRtpReceiverObserver for each RtpReceiver returned by
  // GetReceivers. They're updated automatically when a remote offer/answer
  // from the fake signaling channel is applied, or when
  // ResetRtpReceiverObservers below is called.
  const std::vector<std::unique_ptr<MockRtpReceiverObserver>>&
  rtp_receiver_observers() {
    return rtp_receiver_observers_;
  }

  void ResetRtpReceiverObservers() {
    rtp_receiver_observers_.clear();
    for (const rtc::scoped_refptr<RtpReceiverInterface>& receiver :
         pc()->GetReceivers()) {
      std::unique_ptr<MockRtpReceiverObserver> observer(
          new MockRtpReceiverObserver(receiver->media_type()));
      receiver->SetObserver(observer.get());
      rtp_receiver_observers_.push_back(std::move(observer));
    }
  }

  cricket::PortAllocator* port_allocator() const { return port_allocator_; }

 private:
  SimPeerConnectionWrapper(const std::string& debug_name,
                           std::unique_ptr<SimNetworkManager> network_manager)
      : debug_name_(debug_name), network_manager_(std::move(network_manager)) {}

  bool Init(const PeerConnectionFactory::Options* options,
            const PeerConnectionInterface::RTCConfiguration* config,
            webrtc::PeerConnectionDependencies dependencies,
            rtc::Thread* network_thread,
            rtc::Thread* worker_thread) {
    // There's an error in this test code if Init ends up being called twice.
    RTC_DCHECK(!peer_connection_);
    RTC_DCHECK(!peer_connection_factory_);

    std::unique_ptr<cricket::PortAllocator> port_allocator(
        new cricket::BasicPortAllocator(network_manager_.get()));
    port_allocator_ = port_allocator.get();
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create();
    if (!fake_audio_capture_module_) {
      return false;
    }
    rtc::Thread* const signaling_thread = rtc::Thread::Current();

    webrtc::PeerConnectionFactoryDependencies pc_factory_dependencies;
    pc_factory_dependencies.network_thread = network_thread;
    pc_factory_dependencies.worker_thread = worker_thread;
    pc_factory_dependencies.signaling_thread = signaling_thread;
    pc_factory_dependencies.media_engine =
        cricket::WebRtcMediaEngineFactory::Create(
            rtc::scoped_refptr<webrtc::AudioDeviceModule>(
                fake_audio_capture_module_),
            webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            webrtc::CreateBuiltinVideoEncoderFactory(),
            webrtc::CreateBuiltinVideoDecoderFactory(), nullptr,
            webrtc::AudioProcessingBuilder().Create());
    pc_factory_dependencies.call_factory = webrtc::CreateCallFactory();
    pc_factory_dependencies.event_log_factory =
        webrtc::CreateRtcEventLogFactory();
    peer_connection_factory_ = webrtc::CreateModularPeerConnectionFactory(
        std::move(pc_factory_dependencies));

    if (!peer_connection_factory_) {
      return false;
    }
    if (options) {
      peer_connection_factory_->SetOptions(*options);
    }
    if (config) {
      sdp_semantics_ = config->sdp_semantics;
    }

    dependencies.allocator = std::move(port_allocator);
    peer_connection_ = CreatePeerConnection(config, std::move(dependencies));
    return peer_connection_.get() != nullptr;
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies) {
    PeerConnectionInterface::RTCConfiguration modified_config;
    // If |config| is null, this will result in a default configuration being
    // used.
    if (config) {
      modified_config = *config;
    }
    // Disable resolution adaptation; we don't want it interfering with the
    // test results.
    // TODO(deadbeef): Do something more robust. Since we're testing for aspect
    // ratios and not specific resolutions, is this even necessary?
    modified_config.set_cpu_adaptation(false);

    dependencies.observer = this;
    return peer_connection_factory_->CreatePeerConnection(
        modified_config, std::move(dependencies));
  }

  void set_signaling_message_receiver(
      SignalingMessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  void set_signaling_delay_ms(int delay_ms) { signaling_delay_ms_ = delay_ms; }

  void set_signal_ice_candidates(bool signal) {
    signal_ice_candidates_ = signal;
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrackInternal(
      webrtc::FakePeriodicVideoSource::Config config) {
    // Set max frame rate to 10fps to reduce the risk of test flakiness.
    // TODO(deadbeef): Do something more robust.
    config.frame_interval_ms = 100;

    video_track_sources_.emplace_back(
        new rtc::RefCountedObject<webrtc::FakePeriodicVideoTrackSource>(
            config, false /* remote */));
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track(
        peer_connection_factory_->CreateVideoTrack(
            rtc::CreateRandomUuid(), video_track_sources_.back()));
    if (!local_video_renderer_) {
      local_video_renderer_.reset(new webrtc::FakeVideoTrackRenderer(track));
    }
    return track;
  }

  void HandleIncomingOffer(const std::string& msg) {
    RTC_LOG(LS_INFO) << debug_name_ << ": HandleIncomingOffer";
    std::unique_ptr<SessionDescriptionInterface> desc =
        webrtc::CreateSessionDescription(SdpType::kOffer, msg);

    EXPECT_TRUE(SetRemoteDescription(std::move(desc)));
    // Setting a remote description may have changed the number of receivers,
    // so reset the receiver observers.
    ResetRtpReceiverObservers();
    if (remote_offer_handler_) {
      remote_offer_handler_();
    }
    auto answer = CreateAnswer();
    ASSERT_NE(nullptr, answer);
    EXPECT_TRUE(SetLocalDescriptionAndSendSdpMessage(std::move(answer)));
  }

  void HandleIncomingAnswer(const std::string& msg) {
    RTC_LOG(LS_INFO) << debug_name_ << ": HandleIncomingAnswer";
    std::unique_ptr<SessionDescriptionInterface> desc =
        webrtc::CreateSessionDescription(SdpType::kAnswer, msg);

    EXPECT_TRUE(SetRemoteDescription(std::move(desc)));
    // Set the RtpReceiverObserver after receivers are created.
    ResetRtpReceiverObservers();
  }

  // Returns null on failure.
  std::unique_ptr<SessionDescriptionInterface> CreateOffer() {
    rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
    pc()->CreateOffer(observer, offer_answer_options_);
    return WaitForDescriptionFromObserver(observer);
  }

  // Returns null on failure.
  std::unique_ptr<SessionDescriptionInterface> CreateAnswer() {
    rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
    pc()->CreateAnswer(observer, offer_answer_options_);
    return WaitForDescriptionFromObserver(observer);
  }

  std::unique_ptr<SessionDescriptionInterface> WaitForDescriptionFromObserver(
      MockCreateSessionDescriptionObserver* observer) {
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout);
    if (!observer->result()) {
      return nullptr;
    }
    auto description = observer->MoveDescription();
    return description;
  }

  // Setting the local description and sending the SDP message over the fake
  // signaling channel are combined into the same method because the SDP
  // message needs to be sent as soon as SetLocalDescription finishes, without
  // waiting for the observer to be called. This ensures that ICE candidates
  // don't outrace the description.
  bool SetLocalDescriptionAndSendSdpMessage(
      std::unique_ptr<SessionDescriptionInterface> desc) {
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
    RTC_LOG(LS_INFO) << debug_name_ << ": SetLocalDescriptionAndSendSdpMessage";
    SdpType type = desc->GetType();
    std::string sdp;
    EXPECT_TRUE(desc->ToString(&sdp));
    pc()->SetLocalDescription(observer, desc.release());
    if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {
      RemoveUnusedVideoRenderers();
    }
    // As mentioned above, we need to send the message immediately after
    // SetLocalDescription.
    SendSdpMessage(type, sdp);
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return true;
  }

  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc) {
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
    RTC_LOG(LS_INFO) << debug_name_ << ": SetRemoteDescription";
    pc()->SetRemoteDescription(observer, desc.release());
    if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {
      RemoveUnusedVideoRenderers();
    }
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer->result();
  }

  // This is a work around to remove unused fake_video_renderers from
  // transceivers that have either stopped or are no longer receiving.
  void RemoveUnusedVideoRenderers() {
    auto transceivers = pc()->GetTransceivers();
    for (auto& transceiver : transceivers) {
      if (transceiver->receiver()->media_type() != cricket::MEDIA_TYPE_VIDEO) {
        continue;
      }
      // Remove fake video renderers from any stopped transceivers.
      if (transceiver->stopped()) {
        auto it =
            fake_video_renderers_.find(transceiver->receiver()->track()->id());
        if (it != fake_video_renderers_.end()) {
          fake_video_renderers_.erase(it);
        }
      }
      // Remove fake video renderers from any transceivers that are no longer
      // receiving.
      if ((transceiver->current_direction() &&
           !webrtc::RtpTransceiverDirectionHasRecv(
               *transceiver->current_direction()))) {
        auto it =
            fake_video_renderers_.find(transceiver->receiver()->track()->id());
        if (it != fake_video_renderers_.end()) {
          fake_video_renderers_.erase(it);
        }
      }
    }
  }

  // Simulate sending a blob of SDP with delay |signaling_delay_ms_| (0 by
  // default).
  void SendSdpMessage(SdpType type, const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelaySdpMessageIfReceiverExists(type, msg);
    } else {
      invoker_.AsyncInvokeDelayed<void>(
          RTC_FROM_HERE, rtc::Thread::Current(),
          rtc::Bind(&SimPeerConnectionWrapper::RelaySdpMessageIfReceiverExists,
                    this, type, msg),
          signaling_delay_ms_);
    }
  }

  void RelaySdpMessageIfReceiverExists(SdpType type, const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveSdpMessage(type, msg);
    }
  }

  // Simulate trickling an ICE candidate with delay |signaling_delay_ms_| (0 by
  // default).
  void SendIceMessage(const std::string& sdp_mid,
                      int sdp_mline_index,
                      const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelayIceMessageIfReceiverExists(sdp_mid, sdp_mline_index, msg);
    } else {
      invoker_.AsyncInvokeDelayed<void>(
          RTC_FROM_HERE, rtc::Thread::Current(),
          rtc::Bind(&SimPeerConnectionWrapper::RelayIceMessageIfReceiverExists,
                    this, sdp_mid, sdp_mline_index, msg),
          signaling_delay_ms_);
    }
  }

  void RelayIceMessageIfReceiverExists(const std::string& sdp_mid,
                                       int sdp_mline_index,
                                       const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveIceMessage(sdp_mid, sdp_mline_index,
                                                     msg);
    }
  }

  // SignalingMessageReceiver callbacks.
  void ReceiveSdpMessage(SdpType type, const std::string& msg) override {
    if (type == SdpType::kOffer) {
      HandleIncomingOffer(msg);
    } else {
      HandleIncomingAnswer(msg);
    }
  }

  void ReceiveIceMessage(const std::string& sdp_mid,
                         int sdp_mline_index,
                         const std::string& msg) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": ReceiveIceMessage";
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, msg, nullptr));
    EXPECT_TRUE(pc()->AddIceCandidate(candidate.get()));
  }

  // PeerConnectionObserver callbacks.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    EXPECT_EQ(pc()->signaling_state(), new_state);
  }
  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override {
    if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      rtc::scoped_refptr<VideoTrackInterface> video_track(
          static_cast<VideoTrackInterface*>(receiver->track().get()));
      ASSERT_TRUE(fake_video_renderers_.find(video_track->id()) ==
                  fake_video_renderers_.end());
      fake_video_renderers_[video_track->id()] =
          absl::make_unique<FakeVideoTrackRenderer>(video_track);
    }
  }
  void OnRemoveTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver) override {
    if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      auto it = fake_video_renderers_.find(receiver->track()->id());
      RTC_DCHECK(it != fake_video_renderers_.end());
      fake_video_renderers_.erase(it);
    }
  }
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    EXPECT_EQ(pc()->ice_connection_state(), new_state);
    ice_connection_state_history_.push_back(new_state);
  }
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    EXPECT_EQ(pc()->ice_gathering_state(), new_state);
    ice_gathering_state_history_.push_back(new_state);
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnIceCandidate";

    if (signaling_message_receiver_ == nullptr || !signal_ice_candidates_) {
      // Remote party may be deleted.
      return;
    }
    const cricket::Candidate& c(candidate->candidate());
    if (c.type() == cricket::LOCAL_PORT_TYPE) {
      webrtc::SimInterface* iface =
          network_manager_->core()->GetInterface(c.address().ipaddr());
      RTC_DCHECK(iface != nullptr);
      network_manager_->core()->CreateAndBindSocketOnDualInterface(
          iface->dual(), c.address().port());
      return;
    }
    std::string ice_sdp;
    SendIceMessage(candidate->sdp_mid(), candidate->sdp_mline_index(), ice_sdp);
  }
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    RTC_NOTREACHED();
  }

  std::string debug_name_;

  std::unique_ptr<SimNetworkManager> network_manager_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;

  cricket::PortAllocator* port_allocator_;
  // Needed to keep track of number of frames sent.
  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;
  // Needed to keep track of number of frames received.
  std::map<std::string, std::unique_ptr<webrtc::FakeVideoTrackRenderer>>
      fake_video_renderers_;
  // Needed to ensure frames aren't received for removed tracks.
  std::vector<std::unique_ptr<webrtc::FakeVideoTrackRenderer>>
      removed_fake_video_renderers_;

  // For remote peer communication.
  SignalingMessageReceiver* signaling_message_receiver_ = nullptr;
  int signaling_delay_ms_ = 0;
  bool signal_ice_candidates_ = true;

  // Store references to the video sources we've created, so that we can stop
  // them, if required.
  std::vector<rtc::scoped_refptr<webrtc::VideoTrackSource>>
      video_track_sources_;
  // |local_video_renderer_| attached to the first created local video track.
  std::unique_ptr<webrtc::FakeVideoTrackRenderer> local_video_renderer_;

  SdpSemantics sdp_semantics_;
  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options_;
  std::function<void()> remote_offer_handler_;

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> rtp_receiver_observers_;

  std::vector<PeerConnectionInterface::IceConnectionState>
      ice_connection_state_history_;
  std::vector<PeerConnectionInterface::IceGatheringState>
      ice_gathering_state_history_;

  rtc::AsyncInvoker invoker_;

  friend class SimPeerConnectionTest;
};

// This helper object is used for both specifying how many audio/video frames
// are expected to be received for a caller/callee. It provides helper functions
// to specify these expectations. The object initially starts in a state of no
// expectations.
class MediaExpectations {
 public:
  enum ExpectFrames {
    kExpectSomeFrames,
    kExpectNoFrames,
    kNoExpectation,
  };

  void ExpectBidirectionalAudioAndVideo() {
    ExpectBidirectionalAudio();
    ExpectBidirectionalVideo();
  }

  void ExpectBidirectionalAudio() {
    CallerExpectsSomeAudio();
    CalleeExpectsSomeAudio();
  }

  void ExpectBidirectionalVideo() {
    CallerExpectsSomeVideo();
    CalleeExpectsSomeVideo();
  }

  // Caller's audio functions.
  void CallerExpectsSomeAudio(
      int expected_audio_frames = kDefaultExpectedAudioFrameCount) {
    caller_audio_expectation_ = kExpectSomeFrames;
    caller_audio_frames_expected_ = expected_audio_frames;
  }

  void CallerExpectsNoAudio() {
    caller_audio_expectation_ = kExpectNoFrames;
    caller_audio_frames_expected_ = 0;
  }

  // Caller's video functions.
  void CallerExpectsSomeVideo(
      int expected_video_frames = kDefaultExpectedVideoFrameCount) {
    caller_video_expectation_ = kExpectSomeFrames;
    caller_video_frames_expected_ = expected_video_frames;
  }

  // Callee's audio functions.
  void CalleeExpectsSomeAudio(
      int expected_audio_frames = kDefaultExpectedAudioFrameCount) {
    callee_audio_expectation_ = kExpectSomeFrames;
    callee_audio_frames_expected_ = expected_audio_frames;
  }

  void CalleeExpectsNoAudio() {
    callee_audio_expectation_ = kExpectNoFrames;
    callee_audio_frames_expected_ = 0;
  }

  // Callee's video functions.
  void CalleeExpectsSomeVideo(
      int expected_video_frames = kDefaultExpectedVideoFrameCount) {
    callee_video_expectation_ = kExpectSomeFrames;
    callee_video_frames_expected_ = expected_video_frames;
  }

  void CalleeExpectsNoVideo() {
    callee_video_expectation_ = kExpectNoFrames;
    callee_video_frames_expected_ = 0;
  }

  ExpectFrames caller_audio_expectation_ = kNoExpectation;
  ExpectFrames caller_video_expectation_ = kNoExpectation;
  ExpectFrames callee_audio_expectation_ = kNoExpectation;
  ExpectFrames callee_video_expectation_ = kNoExpectation;
  int caller_audio_frames_expected_ = 0;
  int caller_video_frames_expected_ = 0;
  int callee_audio_frames_expected_ = 0;
  int callee_video_frames_expected_ = 0;
};

class SimPeerConnectionTest : public testing::Test {
 public:
  SimPeerConnectionTest()
      : core_(absl::make_unique<SimCore>()),
        network_thread_(rtc::Thread::CreateWithSocketServer()),
        worker_thread_(rtc::Thread::CreateWithSocketServer()) {
    network_thread_->SetName("PCNetworkThread", this);
    worker_thread_->SetName("PCWorkerThread", this);
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());
    webrtc::metrics::Reset();

    SimConfig config;
    config.webrtc_network_thread = network_thread_.get();
    config.iface_configs.emplace_back(iface_config1);
    config.iface_configs.emplace_back(iface_config2);
    config.iface_configs.emplace_back(iface_config3);
    config.link_configs.emplace_back(link_config1);
    core_->Init(config);
    invoker_.AsyncInvoke<bool>(RTC_FROM_HERE, core_->nio_thread(),
                               rtc::Bind(&SimCore::Start, core_.get()));
  }

  ~SimPeerConnectionTest() {
    // The PeerConnections should deleted before the TurnCustomizers.
    // A TurnPort is created with a raw pointer to a TurnCustomizer. The
    // TurnPort has the same lifetime as the PeerConnection, so it's expected
    // that the TurnCustomizer outlives the life of the PeerConnection or else
    // when Send() is called it will hit a seg fault.
    if (caller_) {
      caller_->set_signaling_message_receiver(nullptr);
      delete SetCallerPcWrapperAndReturnCurrent(nullptr);
    }
    if (callee_) {
      callee_->set_signaling_message_receiver(nullptr);
      delete SetCalleePcWrapperAndReturnCurrent(nullptr);
    }

    // If turn servers were created for the test they need to be destroyed on
    // the network thread.
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
      turn_servers_.clear();
      turn_customizers_.clear();
    });

    core_->Stop();
  }

  bool SignalingStateStable() {
    return caller_->SignalingStateStable() && callee_->SignalingStateStable();
  }

  bool DtlsConnected() {
    // TODO(deadbeef): kIceConnectionConnected currently means both ICE and DTLS
    // are connected. This is an important distinction. Once we have separate
    // ICE and DTLS state, this check needs to use the DTLS state.
    return (callee()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionConnected ||
            callee()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionCompleted) &&
           (caller()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionConnected ||
            caller()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionCompleted);
  }

  std::unique_ptr<SimPeerConnectionWrapper> CreatePeerConnectionWrapper(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies,
      std::unique_ptr<SimNetworkManager> network_manager) {
    RTCConfiguration modified_config;
    if (config) {
      modified_config = *config;
    }
    modified_config.sdp_semantics = sdp_semantics_;
    if (!dependencies.cert_generator) {
      dependencies.cert_generator =
          absl::make_unique<FakeRTCCertificateGenerator>();
    }
    std::unique_ptr<SimPeerConnectionWrapper> client(
        new SimPeerConnectionWrapper(debug_name, std::move(network_manager)));

    if (!client->Init(options, &modified_config, std::move(dependencies),
                      network_thread_.get(), worker_thread_.get())) {
      return nullptr;
    }
    return client;
  }

  bool CreatePeerConnectionWrappers() {
    PeerConnectionInterface::RTCConfiguration config;
    config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
    config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
    return CreatePeerConnectionWrappersWithConfig(config, config);
  }

  bool CreatePeerConnectionWrappersWithConfig(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      const PeerConnectionInterface::RTCConfiguration& callee_config) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, &caller_config,
        webrtc::PeerConnectionDependencies(nullptr),
        core_->CreateNetworkManager({"tun1", "tun2"}));
    callee_ =
        CreatePeerConnectionWrapper("Callee", nullptr, &callee_config,
                                    webrtc::PeerConnectionDependencies(nullptr),
                                    core_->CreateNetworkManager({"tun3"}));
    return caller_ && callee_;
  }

  // Once called, SDP blobs and ICE candidates will be automatically signaled
  // between PeerConnections.
  void ConnectFakeSignaling() {
    caller_->set_signaling_message_receiver(callee_.get());
    callee_->set_signaling_message_receiver(caller_.get());
  }

  void SetSignalingDelayMs(int delay_ms) {
    caller_->set_signaling_delay_ms(delay_ms);
    callee_->set_signaling_delay_ms(delay_ms);
  }

  void SetSignalIceCandidates(bool signal) {
    caller_->set_signal_ice_candidates(signal);
    callee_->set_signal_ice_candidates(signal);
  }

  // Messages may get lost on the unreliable DataChannel, so we send multiple
  // times to avoid test flakiness.
  void SendRtpDataWithRetries(webrtc::DataChannelInterface* dc,
                              const std::string& data,
                              int retries) {
    for (int i = 0; i < retries; ++i) {
      dc->Send(DataBuffer(data));
    }
  }

  rtc::Thread* network_thread() { return network_thread_.get(); }

  SimPeerConnectionWrapper* caller() { return caller_.get(); }

  // Set the |caller_| to the |wrapper| passed in and return the
  // original |caller_|.
  SimPeerConnectionWrapper* SetCallerPcWrapperAndReturnCurrent(
      SimPeerConnectionWrapper* wrapper) {
    SimPeerConnectionWrapper* old = caller_.release();
    caller_.reset(wrapper);
    return old;
  }

  SimPeerConnectionWrapper* callee() { return callee_.get(); }

  // Set the |callee_| to the |wrapper| passed in and return the
  // original |callee_|.
  SimPeerConnectionWrapper* SetCalleePcWrapperAndReturnCurrent(
      SimPeerConnectionWrapper* wrapper) {
    SimPeerConnectionWrapper* old = callee_.release();
    callee_.reset(wrapper);
    return old;
  }

  // Expects the provided number of new frames to be received within
  // kMaxWaitForFramesMs. The new expected frames are specified in
  // |media_expectations|. Returns false if any of the expectations were
  // not met.
  bool ExpectNewFrames(const MediaExpectations& media_expectations) {
    // First initialize the expected frame counts based upon the current
    // frame count.
    int total_caller_audio_frames_expected = caller()->audio_frames_received();
    if (media_expectations.caller_audio_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_caller_audio_frames_expected +=
          media_expectations.caller_audio_frames_expected_;
    }
    int total_caller_video_frames_expected =
        caller()->min_video_frames_received_per_track();
    if (media_expectations.caller_video_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_caller_video_frames_expected +=
          media_expectations.caller_video_frames_expected_;
    }
    int total_callee_audio_frames_expected = callee()->audio_frames_received();
    if (media_expectations.callee_audio_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_callee_audio_frames_expected +=
          media_expectations.callee_audio_frames_expected_;
    }
    int total_callee_video_frames_expected =
        callee()->min_video_frames_received_per_track();
    if (media_expectations.callee_video_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_callee_video_frames_expected +=
          media_expectations.callee_video_frames_expected_;
    }

    // Wait for the expected frames.
    EXPECT_TRUE_WAIT(caller()->audio_frames_received() >=
                             total_caller_audio_frames_expected &&
                         caller()->min_video_frames_received_per_track() >=
                             total_caller_video_frames_expected &&
                         callee()->audio_frames_received() >=
                             total_callee_audio_frames_expected &&
                         callee()->min_video_frames_received_per_track() >=
                             total_callee_video_frames_expected,
                     kMaxWaitForFramesMs);
    bool expectations_correct =
        caller()->audio_frames_received() >=
            total_caller_audio_frames_expected &&
        caller()->min_video_frames_received_per_track() >=
            total_caller_video_frames_expected &&
        callee()->audio_frames_received() >=
            total_callee_audio_frames_expected &&
        callee()->min_video_frames_received_per_track() >=
            total_callee_video_frames_expected;

    // After the combined wait, print out a more detailed message upon
    // failure.
    EXPECT_GE(caller()->audio_frames_received(),
              total_caller_audio_frames_expected);
    EXPECT_GE(caller()->min_video_frames_received_per_track(),
              total_caller_video_frames_expected);
    EXPECT_GE(callee()->audio_frames_received(),
              total_callee_audio_frames_expected);
    EXPECT_GE(callee()->min_video_frames_received_per_track(),
              total_callee_video_frames_expected);

    // We want to make sure nothing unexpected was received.
    if (media_expectations.caller_audio_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(caller()->audio_frames_received(),
                total_caller_audio_frames_expected);
      if (caller()->audio_frames_received() !=
          total_caller_audio_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.caller_video_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(caller()->min_video_frames_received_per_track(),
                total_caller_video_frames_expected);
      if (caller()->min_video_frames_received_per_track() !=
          total_caller_video_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.callee_audio_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(callee()->audio_frames_received(),
                total_callee_audio_frames_expected);
      if (callee()->audio_frames_received() !=
          total_callee_audio_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.callee_video_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(callee()->min_video_frames_received_per_track(),
                total_callee_video_frames_expected);
      if (callee()->min_video_frames_received_per_track() !=
          total_callee_video_frames_expected) {
        expectations_correct = false;
      }
    }
    return expectations_correct;
  }

 protected:
  rtc::AsyncInvoker invoker_;
  std::unique_ptr<SimCore> core_;
  SdpSemantics sdp_semantics_ = SdpSemantics::kUnifiedPlan;

 private:
  // |network_thread_| and |worker_thread_| are used by both
  // |caller_| and |callee_| so they must be destroyed
  // later.
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  // The turn servers and turn customizers should be accessed & deleted on the
  // network thread to avoid a race with the socket read/write that occurs
  // on the network thread.
  std::vector<std::unique_ptr<cricket::TestTurnServer>> turn_servers_;
  std::vector<std::unique_ptr<cricket::TestTurnCustomizer>> turn_customizers_;
  std::unique_ptr<SimPeerConnectionWrapper> caller_;
  std::unique_ptr<SimPeerConnectionWrapper> callee_;
};

// Basic end-to-end test, verifying media can be encoded/transmitted/decoded
// between two connections, using DTLS-SRTP.
TEST_F(SimPeerConnectionTest, EndToEndCallWithDtls) {
  EXPECT_TRUE_WAIT(core_->started(), 1000);
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

}  // namespace

#endif  // if !defined(THREAD_SANITIZER)
