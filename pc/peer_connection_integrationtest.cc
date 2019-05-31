/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
#if !defined(THREAD_SANITIZER)

#include <stdio.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/peer_connection_proxy.h"
#include "api/rtp_receiver_interface.h"
#include "api/test/loopback_media_transport.h"
#include "api/uma_metrics.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/call.h"
#include "logging/rtc_event_log/fake_rtc_event_log_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "media/engine/fake_webrtc_video_engine.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/base/mock_async_resolver.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/test_stun_server.h"
#include "p2p/base/test_turn_customizer.h"
#include "p2p/base/test_turn_server.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/dtmf_sender.h"
#include "pc/local_audio_source.h"
#include "pc/media_session.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_factory.h"
#include "pc/rtp_media_utils.h"
#include "pc/session_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_periodic_video_track_source.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "pc/test/fake_video_track_renderer.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_mdns_responder.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/firewall_socket_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/test_certificate_verifier.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

using ::cricket::ContentInfo;
using ::cricket::StreamParams;
using ::rtc::SocketAddress;
using ::testing::_;
using ::testing::Combine;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

static const int kDefaultTimeout = 10000;
static const int kMaxWaitForStatsMs = 3000;
static const int kMaxWaitForActivationMs = 5000;
static const int kMaxWaitForFramesMs = 10000;
// Default number of audio/video frames to wait for before considering a test
// successful.
static const int kDefaultExpectedAudioFrameCount = 3;
static const int kDefaultExpectedVideoFrameCount = 3;

static const char kDataChannelLabel[] = "data_channel";

// SRTP cipher name negotiated by the tests. This must be updated if the
// default changes.
static const int kDefaultSrtpCryptoSuite = rtc::SRTP_AES128_CM_SHA1_80;
static const int kDefaultSrtpCryptoSuiteGcm = rtc::SRTP_AEAD_AES_256_GCM;

static const SocketAddress kDefaultLocalAddress("192.168.1.1", 0);

// Helper function for constructing offer/answer options to initiate an ICE
// restart.
PeerConnectionInterface::RTCOfferAnswerOptions IceRestartOfferAnswerOptions() {
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.ice_restart = true;
  return options;
}

// Remove all stream information (SSRCs, track IDs, etc.) and "msid-semantic"
// attribute from received SDP, simulating a legacy endpoint.
void RemoveSsrcsAndMsids(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    content.media_description()->mutable_streams().clear();
  }
  desc->set_msid_supported(false);
  desc->set_msid_signaling(0);
}

// Removes all stream information besides the stream ids, simulating an
// endpoint that only signals a=msid lines to convey stream_ids.
void RemoveSsrcsAndKeepMsids(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    std::string track_id;
    std::vector<std::string> stream_ids;
    if (!content.media_description()->streams().empty()) {
      const StreamParams& first_stream =
          content.media_description()->streams()[0];
      track_id = first_stream.id;
      stream_ids = first_stream.stream_ids();
    }
    content.media_description()->mutable_streams().clear();
    StreamParams new_stream;
    new_stream.id = track_id;
    new_stream.set_stream_ids(stream_ids);
    content.media_description()->AddStream(new_stream);
  }
}

int FindFirstMediaStatsIndexByKind(
    const std::string& kind,
    const std::vector<const webrtc::RTCMediaStreamTrackStats*>&
        media_stats_vec) {
  for (size_t i = 0; i < media_stats_vec.size(); i++) {
    if (media_stats_vec[i]->kind.ValueToString() == kind) {
      return i;
    }
  }
  return -1;
}

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

// Helper class that wraps a peer connection, observes it, and can accept
// signaling messages from another wrapper.
//
// Uses a fake network, fake A/V capture, and optionally fake
// encoders/decoders, though they aren't used by default since they don't
// advertise support of any codecs.
// TODO(steveanton): See how this could become a subclass of
// PeerConnectionWrapper defined in peerconnectionwrapper.h.
class PeerConnectionWrapper : public webrtc::PeerConnectionObserver,
                              public SignalingMessageReceiver {
 public:
  // Different factory methods for convenience.
  // TODO(deadbeef): Could use the pattern of:
  //
  // PeerConnectionWrapper =
  //     WrapperBuilder.WithConfig(...).WithOptions(...).build();
  //
  // To reduce some code duplication.
  static PeerConnectionWrapper* CreateWithDtlsIdentityStore(
      const std::string& debug_name,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    PeerConnectionWrapper* client(new PeerConnectionWrapper(debug_name));
    webrtc::PeerConnectionDependencies dependencies(nullptr);
    dependencies.cert_generator = std::move(cert_generator);
    if (!client->Init(nullptr, nullptr, std::move(dependencies), network_thread,
                      worker_thread, nullptr,
                      /*media_transport_factory=*/nullptr)) {
      delete client;
      return nullptr;
    }
    return client;
  }

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

  // Set a callback to be invoked when SDP is received via the fake signaling
  // channel, which provides an opportunity to munge (modify) the SDP. This is
  // used to test SDP being applied that a PeerConnection would normally not
  // generate, but a non-JSEP endpoint might.
  void SetReceivedSdpMunger(
      std::function<void(cricket::SessionDescription*)> munger) {
    received_sdp_munger_ = std::move(munger);
  }

  // Similar to the above, but this is run on SDP immediately after it's
  // generated.
  void SetGeneratedSdpMunger(
      std::function<void(cricket::SessionDescription*)> munger) {
    generated_sdp_munger_ = std::move(munger);
  }

  // Set a callback to be invoked when a remote offer is received via the fake
  // signaling channel. This provides an opportunity to change the
  // PeerConnection state before an answer is created and sent to the caller.
  void SetRemoteOfferHandler(std::function<void()> handler) {
    remote_offer_handler_ = std::move(handler);
  }

  void SetRemoteAsyncResolver(rtc::MockAsyncResolver* resolver) {
    remote_async_resolver_ = resolver;
  }

  // Every ICE connection state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceConnectionState>
  ice_connection_state_history() const {
    return ice_connection_state_history_;
  }
  void clear_ice_connection_state_history() {
    ice_connection_state_history_.clear();
  }

  // Every standardized ICE connection state in order that has been seen by the
  // observer.
  std::vector<PeerConnectionInterface::IceConnectionState>
  standardized_ice_connection_state_history() const {
    return standardized_ice_connection_state_history_;
  }

  // Every PeerConnection state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::PeerConnectionState>
  peer_connection_state_history() const {
    return peer_connection_state_history_;
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
    for (const auto& receiver : pc()->GetReceivers()) {
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

  void CreateDataChannel() { CreateDataChannel(nullptr); }

  void CreateDataChannel(const webrtc::DataChannelInit* init) {
    CreateDataChannel(kDataChannelLabel, init);
  }

  void CreateDataChannel(const std::string& label,
                         const webrtc::DataChannelInit* init) {
    data_channel_ = pc()->CreateDataChannel(label, init);
    ASSERT_TRUE(data_channel_.get() != nullptr);
    data_observer_.reset(new MockDataChannelObserver(data_channel_));
  }

  DataChannelInterface* data_channel() { return data_channel_; }
  const MockDataChannelObserver* data_observer() const {
    return data_observer_.get();
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

  // Returns a MockStatsObserver in a state after stats gathering finished,
  // which can be used to access the gathered stats.
  rtc::scoped_refptr<MockStatsObserver> OldGetStatsForTrack(
      webrtc::MediaStreamTrackInterface* track) {
    rtc::scoped_refptr<MockStatsObserver> observer(
        new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, nullptr, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer;
  }

  // Version that doesn't take a track "filter", and gathers all stats.
  rtc::scoped_refptr<MockStatsObserver> OldGetStats() {
    return OldGetStatsForTrack(nullptr);
  }

  // Synchronously gets stats and returns them. If it times out, fails the test
  // and returns null.
  rtc::scoped_refptr<const webrtc::RTCStatsReport> NewGetStats() {
    rtc::scoped_refptr<webrtc::MockRTCStatsCollectorCallback> callback(
        new rtc::RefCountedObject<webrtc::MockRTCStatsCollectorCallback>());
    peer_connection_->GetStats(callback);
    EXPECT_TRUE_WAIT(callback->called(), kDefaultTimeout);
    return callback->report();
  }

  int rendered_width() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? 0
               : fake_video_renderers_.begin()->second->width();
  }

  int rendered_height() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? 0
               : fake_video_renderers_.begin()->second->height();
  }

  double rendered_aspect_ratio() {
    if (rendered_height() == 0) {
      return 0.0;
    }
    return static_cast<double>(rendered_width()) / rendered_height();
  }

  webrtc::VideoRotation rendered_rotation() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? webrtc::kVideoRotation_0
               : fake_video_renderers_.begin()->second->rotation();
  }

  int local_rendered_width() {
    return local_video_renderer_ ? local_video_renderer_->width() : 0;
  }

  int local_rendered_height() {
    return local_video_renderer_ ? local_video_renderer_->height() : 0;
  }

  double local_rendered_aspect_ratio() {
    if (local_rendered_height() == 0) {
      return 0.0;
    }
    return static_cast<double>(local_rendered_width()) /
           local_rendered_height();
  }

  size_t number_of_remote_streams() {
    if (!pc()) {
      return 0;
    }
    return pc()->remote_streams()->count();
  }

  StreamCollectionInterface* remote_streams() const {
    if (!pc()) {
      ADD_FAILURE();
      return nullptr;
    }
    return pc()->remote_streams();
  }

  StreamCollectionInterface* local_streams() {
    if (!pc()) {
      ADD_FAILURE();
      return nullptr;
    }
    return pc()->local_streams();
  }

  webrtc::PeerConnectionInterface::SignalingState signaling_state() {
    return pc()->signaling_state();
  }

  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state() {
    return pc()->ice_connection_state();
  }

  webrtc::PeerConnectionInterface::IceConnectionState
  standardized_ice_connection_state() {
    return pc()->standardized_ice_connection_state();
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

  rtc::FakeNetworkManager* network_manager() const {
    return fake_network_manager_.get();
  }
  cricket::PortAllocator* port_allocator() const { return port_allocator_; }

  webrtc::FakeRtcEventLogFactory* event_log_factory() const {
    return event_log_factory_;
  }

  const cricket::Candidate& last_candidate_gathered() const {
    return last_candidate_gathered_;
  }

  // Sets the mDNS responder for the owned fake network manager and keeps a
  // reference to the responder.
  void SetMdnsResponder(
      std::unique_ptr<webrtc::FakeMdnsResponder> mdns_responder) {
    RTC_DCHECK(mdns_responder != nullptr);
    mdns_responder_ = mdns_responder.get();
    network_manager()->set_mdns_responder(std::move(mdns_responder));
  }

 private:
  explicit PeerConnectionWrapper(const std::string& debug_name)
      : debug_name_(debug_name) {}

  bool Init(
      const PeerConnectionFactory::Options* options,
      const PeerConnectionInterface::RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread,
      std::unique_ptr<webrtc::FakeRtcEventLogFactory> event_log_factory,
      std::unique_ptr<webrtc::MediaTransportFactory> media_transport_factory) {
    // There's an error in this test code if Init ends up being called twice.
    RTC_DCHECK(!peer_connection_);
    RTC_DCHECK(!peer_connection_factory_);

    fake_network_manager_.reset(new rtc::FakeNetworkManager());
    fake_network_manager_->AddInterface(kDefaultLocalAddress);

    std::unique_ptr<cricket::PortAllocator> port_allocator(
        new cricket::BasicPortAllocator(fake_network_manager_.get()));
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
    if (event_log_factory) {
      event_log_factory_ = event_log_factory.get();
      pc_factory_dependencies.event_log_factory = std::move(event_log_factory);
    } else {
      pc_factory_dependencies.event_log_factory =
          webrtc::CreateRtcEventLogFactory();
    }
    if (media_transport_factory) {
      pc_factory_dependencies.media_transport_factory =
          std::move(media_transport_factory);
    }
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
    if (received_sdp_munger_) {
      received_sdp_munger_(desc->description());
    }

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
    if (received_sdp_munger_) {
      received_sdp_munger_(desc->description());
    }

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
    if (generated_sdp_munger_) {
      generated_sdp_munger_(description->description());
    }
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
          rtc::Bind(&PeerConnectionWrapper::RelaySdpMessageIfReceiverExists,
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
          rtc::Bind(&PeerConnectionWrapper::RelayIceMessageIfReceiverExists,
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
  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    standardized_ice_connection_state_history_.push_back(new_state);
  }
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override {
    peer_connection_state_history_.push_back(new_state);
  }

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    EXPECT_EQ(pc()->ice_gathering_state(), new_state);
    ice_gathering_state_history_.push_back(new_state);
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnIceCandidate";

    if (remote_async_resolver_) {
      const auto& local_candidate = candidate->candidate();
      if (local_candidate.address().IsUnresolvedIP()) {
        RTC_DCHECK(local_candidate.type() == cricket::LOCAL_PORT_TYPE);
        rtc::SocketAddress resolved_addr(local_candidate.address());
        const auto resolved_ip = mdns_responder_->GetMappedAddressForName(
            local_candidate.address().hostname());
        RTC_DCHECK(!resolved_ip.IsNil());
        resolved_addr.SetResolvedIP(resolved_ip);
        EXPECT_CALL(*remote_async_resolver_, GetResolvedAddress(_, _))
            .WillOnce(DoAll(SetArgPointee<1>(resolved_addr), Return(true)));
        EXPECT_CALL(*remote_async_resolver_, Destroy(_));
      }
    }

    std::string ice_sdp;
    EXPECT_TRUE(candidate->ToString(&ice_sdp));
    if (signaling_message_receiver_ == nullptr || !signal_ice_candidates_) {
      // Remote party may be deleted.
      return;
    }
    SendIceMessage(candidate->sdp_mid(), candidate->sdp_mline_index(), ice_sdp);
    last_candidate_gathered_ = candidate->candidate();
  }
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnDataChannel";
    data_channel_ = data_channel;
    data_observer_.reset(new MockDataChannelObserver(data_channel));
  }

  std::string debug_name_;

  std::unique_ptr<rtc::FakeNetworkManager> fake_network_manager_;
  // Reference to the mDNS responder owned by |fake_network_manager_| after set.
  webrtc::FakeMdnsResponder* mdns_responder_ = nullptr;

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
  cricket::Candidate last_candidate_gathered_;

  // Store references to the video sources we've created, so that we can stop
  // them, if required.
  std::vector<rtc::scoped_refptr<webrtc::VideoTrackSource>>
      video_track_sources_;
  // |local_video_renderer_| attached to the first created local video track.
  std::unique_ptr<webrtc::FakeVideoTrackRenderer> local_video_renderer_;

  SdpSemantics sdp_semantics_;
  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options_;
  std::function<void(cricket::SessionDescription*)> received_sdp_munger_;
  std::function<void(cricket::SessionDescription*)> generated_sdp_munger_;
  std::function<void()> remote_offer_handler_;
  rtc::MockAsyncResolver* remote_async_resolver_ = nullptr;
  rtc::scoped_refptr<DataChannelInterface> data_channel_;
  std::unique_ptr<MockDataChannelObserver> data_observer_;

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> rtp_receiver_observers_;

  std::vector<PeerConnectionInterface::IceConnectionState>
      ice_connection_state_history_;
  std::vector<PeerConnectionInterface::IceConnectionState>
      standardized_ice_connection_state_history_;
  std::vector<PeerConnectionInterface::PeerConnectionState>
      peer_connection_state_history_;
  std::vector<PeerConnectionInterface::IceGatheringState>
      ice_gathering_state_history_;

  webrtc::FakeRtcEventLogFactory* event_log_factory_;

  rtc::AsyncInvoker invoker_;

  friend class PeerConnectionIntegrationTestFixture;
};

class MockRtcEventLogOutput : public webrtc::RtcEventLogOutput {
 public:
  virtual ~MockRtcEventLogOutput() = default;
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_METHOD1(Write, bool(const std::string&));
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

  void ExpectNoAudio() {
    CallerExpectsNoAudio();
    CalleeExpectsNoAudio();
  }

  void ExpectBidirectionalVideo() {
    CallerExpectsSomeVideo();
    CalleeExpectsSomeVideo();
  }

  void ExpectNoVideo() {
    CallerExpectsNoVideo();
    CalleeExpectsNoVideo();
  }

  void CallerExpectsSomeAudioAndVideo() {
    CallerExpectsSomeAudio();
    CallerExpectsSomeVideo();
  }

  void CalleeExpectsSomeAudioAndVideo() {
    CalleeExpectsSomeAudio();
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

  void CallerExpectsNoVideo() {
    caller_video_expectation_ = kExpectNoFrames;
    caller_video_frames_expected_ = 0;
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

// Tests two PeerConnections connecting to each other end-to-end, using a
// virtual network, fake A/V capture and fake encoder/decoders. The
// PeerConnections share the threads/socket servers, but use separate versions
// of everything else (including "PeerConnectionFactory"s).
class PeerConnectionIntegrationTestFixture {
 public:
  explicit PeerConnectionIntegrationTestFixture(SdpSemantics sdp_semantics)
      : sdp_semantics_(sdp_semantics),
        ss_(new rtc::VirtualSocketServer()),
        fss_(new rtc::FirewallSocketServer(ss_.get())),
        network_thread_(new rtc::Thread(fss_.get())),
        worker_thread_(rtc::Thread::Create()),
        loopback_media_transports_(network_thread_.get()) {
    network_thread_->SetName("PCNetworkThread", this);
    worker_thread_->SetName("PCWorkerThread", this);
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());
    webrtc::metrics::Reset();
  }

  ~PeerConnectionIntegrationTestFixture() {
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

  // When |event_log_factory| is null, the default implementation of the event
  // log factory will be used.
  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWrapper(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies,
      std::unique_ptr<webrtc::FakeRtcEventLogFactory> event_log_factory,
      std::unique_ptr<webrtc::MediaTransportFactory> media_transport_factory) {
    RTCConfiguration modified_config;
    if (config) {
      modified_config = *config;
    }
    modified_config.sdp_semantics = sdp_semantics_;
    if (!dependencies.cert_generator) {
      dependencies.cert_generator =
          absl::make_unique<FakeRTCCertificateGenerator>();
    }
    std::unique_ptr<PeerConnectionWrapper> client(
        new PeerConnectionWrapper(debug_name));

    if (!client->Init(options, &modified_config, std::move(dependencies),
                      network_thread_.get(), worker_thread_.get(),
                      std::move(event_log_factory),
                      std::move(media_transport_factory))) {
      return nullptr;
    }
    return client;
  }

  std::unique_ptr<PeerConnectionWrapper>
  CreatePeerConnectionWrapperWithFakeRtcEventLog(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies) {
    std::unique_ptr<webrtc::FakeRtcEventLogFactory> event_log_factory(
        new webrtc::FakeRtcEventLogFactory(rtc::Thread::Current()));
    return CreatePeerConnectionWrapper(debug_name, options, config,
                                       std::move(dependencies),
                                       std::move(event_log_factory),
                                       /*media_transport_factory=*/nullptr);
  }

  bool CreatePeerConnectionWrappers() {
    return CreatePeerConnectionWrappersWithConfig(
        PeerConnectionInterface::RTCConfiguration(),
        PeerConnectionInterface::RTCConfiguration());
  }

  bool CreatePeerConnectionWrappersWithSdpSemantics(
      SdpSemantics caller_semantics,
      SdpSemantics callee_semantics) {
    // Can't specify the sdp_semantics in the passed-in configuration since it
    // will be overwritten by CreatePeerConnectionWrapper with whatever is
    // stored in sdp_semantics_. So get around this by modifying the instance
    // variable before calling CreatePeerConnectionWrapper for the caller and
    // callee PeerConnections.
    SdpSemantics original_semantics = sdp_semantics_;
    sdp_semantics_ = caller_semantics;
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, nullptr, webrtc::PeerConnectionDependencies(nullptr),
        nullptr, /*media_transport_factory=*/nullptr);
    sdp_semantics_ = callee_semantics;
    callee_ = CreatePeerConnectionWrapper(
        "Callee", nullptr, nullptr, webrtc::PeerConnectionDependencies(nullptr),
        nullptr, /*media_transport_factory=*/nullptr);
    sdp_semantics_ = original_semantics;
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfig(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      const PeerConnectionInterface::RTCConfiguration& callee_config) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, &caller_config,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*media_transport_factory=*/nullptr);
    callee_ = CreatePeerConnectionWrapper(
        "Callee", nullptr, &callee_config,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*media_transport_factory=*/nullptr);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      const PeerConnectionInterface::RTCConfiguration& callee_config,
      std::unique_ptr<webrtc::MediaTransportFactory> caller_factory,
      std::unique_ptr<webrtc::MediaTransportFactory> callee_factory) {
    caller_ =
        CreatePeerConnectionWrapper("Caller", nullptr, &caller_config,
                                    webrtc::PeerConnectionDependencies(nullptr),
                                    nullptr, std::move(caller_factory));
    callee_ =
        CreatePeerConnectionWrapper("Callee", nullptr, &callee_config,
                                    webrtc::PeerConnectionDependencies(nullptr),
                                    nullptr, std::move(callee_factory));
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfigAndDeps(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      webrtc::PeerConnectionDependencies caller_dependencies,
      const PeerConnectionInterface::RTCConfiguration& callee_config,
      webrtc::PeerConnectionDependencies callee_dependencies) {
    caller_ =
        CreatePeerConnectionWrapper("Caller", nullptr, &caller_config,
                                    std::move(caller_dependencies), nullptr,
                                    /*media_transport_factory=*/nullptr);
    callee_ =
        CreatePeerConnectionWrapper("Callee", nullptr, &callee_config,
                                    std::move(callee_dependencies), nullptr,
                                    /*media_transport_factory=*/nullptr);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithOptions(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", &caller_options, nullptr,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*media_transport_factory=*/nullptr);
    callee_ = CreatePeerConnectionWrapper(
        "Callee", &callee_options, nullptr,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*media_transport_factory=*/nullptr);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithFakeRtcEventLog() {
    PeerConnectionInterface::RTCConfiguration default_config;
    caller_ = CreatePeerConnectionWrapperWithFakeRtcEventLog(
        "Caller", nullptr, &default_config,
        webrtc::PeerConnectionDependencies(nullptr));
    callee_ = CreatePeerConnectionWrapperWithFakeRtcEventLog(
        "Callee", nullptr, &default_config,
        webrtc::PeerConnectionDependencies(nullptr));
    return caller_ && callee_;
  }

  std::unique_ptr<PeerConnectionWrapper>
  CreatePeerConnectionWrapperWithAlternateKey() {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    cert_generator->use_alternate_key();

    webrtc::PeerConnectionDependencies dependencies(nullptr);
    dependencies.cert_generator = std::move(cert_generator);
    return CreatePeerConnectionWrapper("New Peer", nullptr, nullptr,
                                       std::move(dependencies), nullptr,
                                       /*media_transport_factory=*/nullptr);
  }

  cricket::TestTurnServer* CreateTurnServer(
      rtc::SocketAddress internal_address,
      rtc::SocketAddress external_address,
      cricket::ProtocolType type = cricket::ProtocolType::PROTO_UDP,
      const std::string& common_name = "test turn server") {
    rtc::Thread* thread = network_thread();
    std::unique_ptr<cricket::TestTurnServer> turn_server =
        network_thread()->Invoke<std::unique_ptr<cricket::TestTurnServer>>(
            RTC_FROM_HERE,
            [thread, internal_address, external_address, type, common_name] {
              return absl::make_unique<cricket::TestTurnServer>(
                  thread, internal_address, external_address, type,
                  /*ignore_bad_certs=*/true, common_name);
            });
    turn_servers_.push_back(std::move(turn_server));
    // Interactions with the turn server should be done on the network thread.
    return turn_servers_.back().get();
  }

  cricket::TestTurnCustomizer* CreateTurnCustomizer() {
    std::unique_ptr<cricket::TestTurnCustomizer> turn_customizer =
        network_thread()->Invoke<std::unique_ptr<cricket::TestTurnCustomizer>>(
            RTC_FROM_HERE,
            [] { return absl::make_unique<cricket::TestTurnCustomizer>(); });
    turn_customizers_.push_back(std::move(turn_customizer));
    // Interactions with the turn customizer should be done on the network
    // thread.
    return turn_customizers_.back().get();
  }

  // Checks that the function counters for a TestTurnCustomizer are greater than
  // 0.
  void ExpectTurnCustomizerCountersIncremented(
      cricket::TestTurnCustomizer* turn_customizer) {
    unsigned int allow_channel_data_counter =
        network_thread()->Invoke<unsigned int>(
            RTC_FROM_HERE, [turn_customizer] {
              return turn_customizer->allow_channel_data_cnt_;
            });
    EXPECT_GT(allow_channel_data_counter, 0u);
    unsigned int modify_counter = network_thread()->Invoke<unsigned int>(
        RTC_FROM_HERE,
        [turn_customizer] { return turn_customizer->modify_cnt_; });
    EXPECT_GT(modify_counter, 0u);
  }

  // Once called, SDP blobs and ICE candidates will be automatically signaled
  // between PeerConnections.
  void ConnectFakeSignaling() {
    caller_->set_signaling_message_receiver(callee_.get());
    callee_->set_signaling_message_receiver(caller_.get());
  }

  // Once called, SDP blobs will be automatically signaled between
  // PeerConnections. Note that ICE candidates will not be signaled unless they
  // are in the exchanged SDP blobs.
  void ConnectFakeSignalingForSdpOnly() {
    ConnectFakeSignaling();
    SetSignalIceCandidates(false);
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

  rtc::VirtualSocketServer* virtual_socket_server() { return ss_.get(); }

  webrtc::MediaTransportPair* loopback_media_transports() {
    return &loopback_media_transports_;
  }

  PeerConnectionWrapper* caller() { return caller_.get(); }

  // Set the |caller_| to the |wrapper| passed in and return the
  // original |caller_|.
  PeerConnectionWrapper* SetCallerPcWrapperAndReturnCurrent(
      PeerConnectionWrapper* wrapper) {
    PeerConnectionWrapper* old = caller_.release();
    caller_.reset(wrapper);
    return old;
  }

  PeerConnectionWrapper* callee() { return callee_.get(); }

  // Set the |callee_| to the |wrapper| passed in and return the
  // original |callee_|.
  PeerConnectionWrapper* SetCalleePcWrapperAndReturnCurrent(
      PeerConnectionWrapper* wrapper) {
    PeerConnectionWrapper* old = callee_.release();
    callee_.reset(wrapper);
    return old;
  }

  void SetPortAllocatorFlags(uint32_t caller_flags, uint32_t callee_flags) {
    network_thread()->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::set_flags,
                                 caller()->port_allocator(), caller_flags));
    network_thread()->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::set_flags,
                                 callee()->port_allocator(), callee_flags));
  }

  rtc::FirewallSocketServer* firewall() const { return fss_.get(); }

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

  void ClosePeerConnections() {
    caller()->pc()->Close();
    callee()->pc()->Close();
  }

  void TestNegotiatedCipherSuite(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options,
      int expected_cipher_suite) {
    ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(caller_options,
                                                        callee_options));
    ConnectFakeSignaling();
    caller()->AddAudioVideoTracks();
    callee()->AddAudioVideoTracks();
    caller()->CreateAndSetAndSignalOffer();
    ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
    EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(expected_cipher_suite),
                   caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
    // TODO(bugs.webrtc.org/9456): Fix it.
    EXPECT_EQ(1, webrtc::metrics::NumEvents(
                     "WebRTC.PeerConnection.SrtpCryptoSuite.Audio",
                     expected_cipher_suite));
  }

  void TestGcmNegotiationUsesCipherSuite(bool local_gcm_enabled,
                                         bool remote_gcm_enabled,
                                         int expected_cipher_suite) {
    PeerConnectionFactory::Options caller_options;
    caller_options.crypto_options.srtp.enable_gcm_crypto_suites =
        local_gcm_enabled;
    PeerConnectionFactory::Options callee_options;
    callee_options.crypto_options.srtp.enable_gcm_crypto_suites =
        remote_gcm_enabled;
    TestNegotiatedCipherSuite(caller_options, callee_options,
                              expected_cipher_suite);
  }

  SdpSemantics sdp_semantics_;

 private:
  // |ss_| is used by |network_thread_| so it must be destroyed later.
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  std::unique_ptr<rtc::FirewallSocketServer> fss_;
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
  webrtc::MediaTransportPair loopback_media_transports_;
  std::unique_ptr<PeerConnectionWrapper> caller_;
  std::unique_ptr<PeerConnectionWrapper> callee_;
};

class PeerConnectionIntegrationTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  SdpSemantics GetSdpSemantics() const { return GetParam(); }
};

class PeerConnectionIntegrationTestPlanB : public ::testing::Test {
 protected:
  SdpSemantics GetSdpSemantics() const { return SdpSemantics::kPlanB; }
};

class PeerConnectionIntegrationTestUnifiedPlan : public ::testing::Test {
 protected:
  SdpSemantics GetSdpSemantics() const { return SdpSemantics::kUnifiedPlan; }
};

// Test the OnFirstPacketReceived callback from audio/video RtpReceivers.  This
// includes testing that the callback is invoked if an observer is connected
// after the first packet has already been received.
TEST_P(PeerConnectionIntegrationTest,
       RtpReceiverObserverOnFirstPacketReceived) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  // Start offer/answer exchange and wait for it to complete.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Should be one receiver each for audio/video.
  EXPECT_EQ(2U, fixture.caller()->rtp_receiver_observers().size());
  EXPECT_EQ(2U, fixture.callee()->rtp_receiver_observers().size());
  // Wait for all "first packet received" callbacks to be fired.
  EXPECT_TRUE_WAIT(
      absl::c_all_of(fixture.caller()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }),
      kMaxWaitForFramesMs);
  EXPECT_TRUE_WAIT(
      absl::c_all_of(fixture.callee()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }),
      kMaxWaitForFramesMs);
  // If new observers are set after the first packet was already received, the
  // callback should still be invoked.
  fixture.caller()->ResetRtpReceiverObservers();
  fixture.callee()->ResetRtpReceiverObservers();
  EXPECT_EQ(2U, fixture.caller()->rtp_receiver_observers().size());
  EXPECT_EQ(2U, fixture.callee()->rtp_receiver_observers().size());
  EXPECT_TRUE(
      absl::c_all_of(fixture.caller()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }));
  EXPECT_TRUE(
      absl::c_all_of(fixture.callee()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }));
}

class DummyDtmfObserver : public DtmfSenderObserverInterface {
 public:
  DummyDtmfObserver() : completed_(false) {}

  // Implements DtmfSenderObserverInterface.
  void OnToneChange(const std::string& tone) override {
    tones_.push_back(tone);
    if (tone.empty()) {
      completed_ = true;
    }
  }

  const std::vector<std::string>& tones() const { return tones_; }
  bool completed() const { return completed_; }

 private:
  bool completed_;
  std::vector<std::string> tones_;
};

// Assumes |sender| already has an audio track added and the offer/answer
// exchange is done.
void TestDtmfFromSenderToReceiver(PeerConnectionWrapper* sender,
                                  PeerConnectionWrapper* receiver) {
  // We should be able to get a DTMF sender from the local sender.
  rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender =
      sender->pc()->GetSenders().at(0)->GetDtmfSender();
  ASSERT_TRUE(dtmf_sender);
  DummyDtmfObserver observer;
  dtmf_sender->RegisterObserver(&observer);

  // Test the DtmfSender object just created.
  EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
  EXPECT_TRUE(dtmf_sender->InsertDtmf("1a", 100, 50));

  EXPECT_TRUE_WAIT(observer.completed(), kDefaultTimeout);
  std::vector<std::string> tones = {"1", "a", ""};
  EXPECT_EQ(tones, observer.tones());
  dtmf_sender->UnregisterObserver();
  // TODO(deadbeef): Verify the tones were actually received end-to-end.
}

// Verifies the DtmfSenderObserver callbacks for a DtmfSender (one in each
// direction).
TEST_P(PeerConnectionIntegrationTest, DtmfSenderObserver) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Only need audio for DTMF.
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // DTLS must finish before the DTMF sender can be used reliably.
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);
  TestDtmfFromSenderToReceiver(fixture.caller(), fixture.callee());
  TestDtmfFromSenderToReceiver(fixture.callee(), fixture.caller());
}

// Basic end-to-end test, verifying media can be encoded/transmitted/decoded
// between two connections, using DTLS-SRTP.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithDtls) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  EXPECT_LE(2, webrtc::metrics::NumEvents("WebRTC.PeerConnection.KeyProtocol",
                                          webrtc::kEnumCounterKeyProtocolDtls));
  EXPECT_EQ(0, webrtc::metrics::NumEvents("WebRTC.PeerConnection.KeyProtocol",
                                          webrtc::kEnumCounterKeyProtocolSdes));
}

// Uses SDES instead of DTLS for key agreement.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithSdes) {
  PeerConnectionInterface::RTCConfiguration sdes_config;
  sdes_config.enable_dtls_srtp.emplace(false);
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfig(sdes_config, sdes_config));
  fixture.ConnectFakeSignaling();

  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  EXPECT_LE(2, webrtc::metrics::NumEvents("WebRTC.PeerConnection.KeyProtocol",
                                          webrtc::kEnumCounterKeyProtocolSdes));
  EXPECT_EQ(0, webrtc::metrics::NumEvents("WebRTC.PeerConnection.KeyProtocol",
                                          webrtc::kEnumCounterKeyProtocolDtls));
}

// Tests that the GetRemoteAudioSSLCertificate method returns the remote DTLS
// certificate once the DTLS handshake has finished.
TEST_P(PeerConnectionIntegrationTest,
       GetRemoteAudioSSLCertificateReturnsExchangedCertificate) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());

  auto GetRemoteAudioSSLCertificate = [](PeerConnectionWrapper* wrapper) {
    auto pci = reinterpret_cast<PeerConnectionProxy*>(wrapper->pc());
    auto pc = reinterpret_cast<PeerConnection*>(pci->internal());
    return pc->GetRemoteAudioSSLCertificate();
  };
  auto GetRemoteAudioSSLCertChain = [](PeerConnectionWrapper* wrapper) {
    auto pci = reinterpret_cast<PeerConnectionProxy*>(wrapper->pc());
    auto pc = reinterpret_cast<PeerConnection*>(pci->internal());
    return pc->GetRemoteAudioSSLCertChain();
  };

  auto caller_cert = rtc::RTCCertificate::FromPEM(kRsaPems[0]);
  auto callee_cert = rtc::RTCCertificate::FromPEM(kRsaPems[1]);

  // Configure each side with a known certificate so they can be compared later.
  PeerConnectionInterface::RTCConfiguration caller_config;
  caller_config.enable_dtls_srtp.emplace(true);
  caller_config.certificates.push_back(caller_cert);
  PeerConnectionInterface::RTCConfiguration callee_config;
  callee_config.enable_dtls_srtp.emplace(true);
  callee_config.certificates.push_back(callee_cert);
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(caller_config,
                                                             callee_config));
  fixture.ConnectFakeSignaling();

  // When first initialized, there should not be a remote SSL certificate (and
  // calling this method should not crash).
  EXPECT_EQ(nullptr, GetRemoteAudioSSLCertificate(fixture.caller()));
  EXPECT_EQ(nullptr, GetRemoteAudioSSLCertificate(fixture.callee()));
  EXPECT_EQ(nullptr, GetRemoteAudioSSLCertChain(fixture.caller()));
  EXPECT_EQ(nullptr, GetRemoteAudioSSLCertChain(fixture.callee()));

  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);

  // Once DTLS has been connected, each side should return the other's SSL
  // certificate when calling GetRemoteAudioSSLCertificate.

  auto caller_remote_cert = GetRemoteAudioSSLCertificate(fixture.caller());
  ASSERT_TRUE(caller_remote_cert);
  EXPECT_EQ(callee_cert->GetSSLCertificate().ToPEMString(),
            caller_remote_cert->ToPEMString());

  auto callee_remote_cert = GetRemoteAudioSSLCertificate(fixture.callee());
  ASSERT_TRUE(callee_remote_cert);
  EXPECT_EQ(caller_cert->GetSSLCertificate().ToPEMString(),
            callee_remote_cert->ToPEMString());

  auto caller_remote_cert_chain = GetRemoteAudioSSLCertChain(fixture.caller());
  ASSERT_TRUE(caller_remote_cert_chain);
  ASSERT_EQ(1U, caller_remote_cert_chain->GetSize());
  auto remote_cert = &caller_remote_cert_chain->Get(0);
  EXPECT_EQ(callee_cert->GetSSLCertificate().ToPEMString(),
            remote_cert->ToPEMString());

  auto callee_remote_cert_chain = GetRemoteAudioSSLCertChain(fixture.callee());
  ASSERT_TRUE(callee_remote_cert_chain);
  ASSERT_EQ(1U, callee_remote_cert_chain->GetSize());
  remote_cert = &callee_remote_cert_chain->Get(0);
  EXPECT_EQ(caller_cert->GetSSLCertificate().ToPEMString(),
            remote_cert->ToPEMString());
}

// This test sets up a call between two parties with a source resolution of
// 1280x720 and verifies that a 16:9 aspect ratio is received.
TEST_P(PeerConnectionIntegrationTest,
       Send1280By720ResolutionAndReceive16To9AspectRatio) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Add video tracks with 16:9 aspect ratio, size 1280 x 720.
  webrtc::FakePeriodicVideoSource::Config config;
  config.width = 1280;
  config.height = 720;
  config.timestamp_offset_ms = rtc::TimeMillis();
  fixture.caller()->AddTrack(
      fixture.caller()->CreateLocalVideoTrackWithConfig(config));
  fixture.callee()->AddTrack(
      fixture.callee()->CreateLocalVideoTrackWithConfig(config));

  // Do normal offer/answer and wait for at least one frame to be received in
  // each direction.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(
      fixture.caller()->min_video_frames_received_per_track() > 0 &&
          fixture.callee()->min_video_frames_received_per_track() > 0,
      kMaxWaitForFramesMs);

  // Check rendered aspect ratio.
  EXPECT_EQ(16.0 / 9, fixture.caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, fixture.caller()->rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, fixture.callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, fixture.callee()->rendered_aspect_ratio());
}

// This test sets up an one-way call, with media only from caller to
// callee.
TEST_P(PeerConnectionIntegrationTest, OneWayMediaCall) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  media_expectations.CallerExpectsNoAudio();
  media_expectations.CallerExpectsNoVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This test sets up a audio call initially, with the callee rejecting video
// initially. Then later the callee decides to upgrade to audio/video, and
// initiates a new offer/answer exchange.
TEST_P(PeerConnectionIntegrationTest, AudioToVideoUpgrade) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Initially, offer an audio/video stream from the caller, but refuse to
  // send/receive video on the callee side.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioTrack();
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 0;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    fixture.callee()->SetRemoteOfferHandler([&] {
      fixture.callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
          ->Stop();
    });
  }
  // Do offer/answer and make sure audio is still received end-to-end.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudio();
    media_expectations.ExpectNoVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
  // Sanity check that the callee's description has a rejected video section.
  ASSERT_NE(nullptr, fixture.callee()->pc()->local_description());
  const ContentInfo* callee_video_content = GetFirstVideoContent(
      fixture.callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);

  // Now negotiate with video and ensure negotiation succeeds, with video
  // frames and additional audio frames being received.
  fixture.callee()->AddVideoTrack();
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 1;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    fixture.callee()->SetRemoteOfferHandler(nullptr);
    fixture.caller()->SetRemoteOfferHandler([&] {
      // The caller creates a new transceiver to receive video on when receiving
      // the offer, but by default it is send only.
      auto transceivers = fixture.caller()->pc()->GetTransceivers();
      ASSERT_EQ(3U, transceivers.size());
      ASSERT_EQ(cricket::MEDIA_TYPE_VIDEO,
                transceivers[2]->receiver()->media_type());
      transceivers[2]->sender()->SetTrack(
          fixture.caller()->CreateLocalVideoTrack());
      transceivers[2]->SetDirection(RtpTransceiverDirection::kSendRecv);
    });
  }
  fixture.callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    // Expect additional audio frames to be received after the upgrade.
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
}

// Simpler than the above test; just add an audio track to an established
// video-only connection.
TEST_P(PeerConnectionIntegrationTest, AddAudioToVideoOnlyCall) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Do initial offer/answer with just a video track.
  fixture.caller()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Now add an audio track and do another offer/answer.
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Ensure both audio and video frames are received end-to-end.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This test sets up a call that's transferred to a new caller with a different
// DTLS fingerprint.
TEST_P(PeerConnectionIntegrationTest, CallTransferredForCallee) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Keep the original peer around which will still send packets to the
  // receiving client. These SRTP packets will be dropped.
  std::unique_ptr<PeerConnectionWrapper> original_peer(
      fixture.SetCallerPcWrapperAndReturnCurrent(
          fixture.CreatePeerConnectionWrapperWithAlternateKey().release()));
  // TODO(deadbeef): Why do we call Close here? That goes against the comment
  // directly above.
  original_peer->pc()->Close();

  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for some additional frames to be transmitted end-to-end.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This test sets up a call that's transferred to a new callee with a different
// DTLS fingerprint.
TEST_P(PeerConnectionIntegrationTest, CallTransferredForCaller) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Keep the original peer around which will still send packets to the
  // receiving client. These SRTP packets will be dropped.
  std::unique_ptr<PeerConnectionWrapper> original_peer(
      fixture.SetCalleePcWrapperAndReturnCurrent(
          fixture.CreatePeerConnectionWrapperWithAlternateKey().release()));
  // TODO(deadbeef): Why do we call Close here? That goes against the comment
  // directly above.
  original_peer->pc()->Close();

  fixture.ConnectFakeSignaling();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for some additional frames to be transmitted end-to-end.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This test sets up a non-bundled call and negotiates bundling at the same
// time as starting an ICE restart. When bundling is in effect in the restart,
// the DTLS-SRTP context should be successfully reset.
TEST_P(PeerConnectionIntegrationTest, BundlingEnabledWhileIceRestartOccurs) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  // Remove the bundle group from the SDP received by the callee.
  fixture.callee()->SetReceivedSdpMunger([](cricket::SessionDescription* desc) {
    desc->RemoveGroupByName("BUNDLE");
  });
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
  // Now stop removing the BUNDLE group, and trigger an ICE restart.
  fixture.callee()->SetReceivedSdpMunger(nullptr);
  fixture.caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Expect additional frames to be received after the ICE restart.
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
}

// Test CVO (Coordination of Video Orientation). If a video source is rotated
// and both peers support the CVO RTP header extension, the actual video frames
// don't need to be encoded in different resolutions, since the rotation is
// communicated through the RTP header extension.
TEST_P(PeerConnectionIntegrationTest, RotatedVideoWithCVOExtension) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Add rotated video tracks.
  fixture.caller()->AddTrack(
      fixture.caller()->CreateLocalVideoTrackWithRotation(
          webrtc::kVideoRotation_90));
  fixture.callee()->AddTrack(
      fixture.callee()->CreateLocalVideoTrackWithRotation(
          webrtc::kVideoRotation_270));

  // Wait for video frames to be received by both sides.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(
      fixture.caller()->min_video_frames_received_per_track() > 0 &&
          fixture.callee()->min_video_frames_received_per_track() > 0,
      kMaxWaitForFramesMs);

  // Ensure that the aspect ratio is unmodified.
  // TODO(deadbeef): Where does 4:3 come from? Should be explicit in the test,
  // not just assumed.
  EXPECT_EQ(4.0 / 3, fixture.caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, fixture.caller()->rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, fixture.callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, fixture.callee()->rendered_aspect_ratio());
  // Ensure that the CVO bits were surfaced to the renderer.
  EXPECT_EQ(webrtc::kVideoRotation_270, fixture.caller()->rendered_rotation());
  EXPECT_EQ(webrtc::kVideoRotation_90, fixture.callee()->rendered_rotation());
}

// Test that when the CVO extension isn't supported, video is rotated the
// old-fashioned way, by encoding rotated frames.
TEST_P(PeerConnectionIntegrationTest, RotatedVideoWithoutCVOExtension) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Add rotated video tracks.
  fixture.caller()->AddTrack(
      fixture.caller()->CreateLocalVideoTrackWithRotation(
          webrtc::kVideoRotation_90));
  fixture.callee()->AddTrack(
      fixture.callee()->CreateLocalVideoTrackWithRotation(
          webrtc::kVideoRotation_270));

  // Remove the CVO extension from the offered SDP.
  fixture.callee()->SetReceivedSdpMunger([](cricket::SessionDescription* desc) {
    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(desc);
    video->ClearRtpHeaderExtensions();
  });
  // Wait for video frames to be received by both sides.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(
      fixture.caller()->min_video_frames_received_per_track() > 0 &&
          fixture.callee()->min_video_frames_received_per_track() > 0,
      kMaxWaitForFramesMs);

  // Expect that the aspect ratio is inversed to account for the 90/270 degree
  // rotation.
  // TODO(deadbeef): Where does 4:3 come from? Should be explicit in the test,
  // not just assumed.
  EXPECT_EQ(3.0 / 4, fixture.caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, fixture.caller()->rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, fixture.callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, fixture.callee()->rendered_aspect_ratio());
  // Expect that each endpoint is unaware of the rotation of the other endpoint.
  EXPECT_EQ(webrtc::kVideoRotation_0, fixture.caller()->rendered_rotation());
  EXPECT_EQ(webrtc::kVideoRotation_0, fixture.callee()->rendered_rotation());
}

// Test that if the answerer rejects the audio m= section, no audio is sent or
// received, but video still can be.
TEST_P(PeerConnectionIntegrationTest, AnswererRejectsAudioSection) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    // Only add video track for callee, and set offer_to_receive_audio to 0, so
    // it will reject the audio m= section completely.
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    // Stopping the audio RtpTransceiver will cause the media section to be
    // rejected in the answer.
    fixture.callee()->SetRemoteOfferHandler([&] {
      fixture.callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_AUDIO)
          ->Stop();
    });
  }
  fixture.callee()->AddTrack(fixture.callee()->CreateLocalVideoTrack());
  // Do offer/answer and wait for successful end-to-end video frames.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  media_expectations.ExpectNoAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // Sanity check that the callee's description has a rejected audio section.
  ASSERT_NE(nullptr, fixture.callee()->pc()->local_description());
  const ContentInfo* callee_audio_content = GetFirstAudioContent(
      fixture.callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_audio_content);
  EXPECT_TRUE(callee_audio_content->rejected);
  if (fixture.sdp_semantics_ == SdpSemantics::kUnifiedPlan) {
    // The caller's transceiver should have stopped after receiving the answer.
    EXPECT_TRUE(fixture.caller()
                    ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_AUDIO)
                    ->stopped());
  }
}

// Test that if the answerer rejects the video m= section, no video is sent or
// received, but audio still can be.
TEST_P(PeerConnectionIntegrationTest, AnswererRejectsVideoSection) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    // Only add audio track for callee, and set offer_to_receive_video to 0, so
    // it will reject the video m= section completely.
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 0;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    // Stopping the video RtpTransceiver will cause the media section to be
    // rejected in the answer.
    fixture.callee()->SetRemoteOfferHandler([&] {
      fixture.callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
          ->Stop();
    });
  }
  fixture.callee()->AddTrack(fixture.callee()->CreateLocalAudioTrack());
  // Do offer/answer and wait for successful end-to-end audio frames.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  media_expectations.ExpectNoVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // Sanity check that the callee's description has a rejected video section.
  ASSERT_NE(nullptr, fixture.callee()->pc()->local_description());
  const ContentInfo* callee_video_content = GetFirstVideoContent(
      fixture.callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
  if (fixture.sdp_semantics_ == SdpSemantics::kUnifiedPlan) {
    // The caller's transceiver should have stopped after receiving the answer.
    EXPECT_TRUE(fixture.caller()
                    ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
                    ->stopped());
  }
}

// Test that if the answerer rejects both audio and video m= sections, nothing
// bad happens.
// TODO(deadbeef): Test that a data channel still works. Currently this doesn't
// test anything but the fact that negotiation succeeds, which doesn't mean
// much.
TEST_P(PeerConnectionIntegrationTest, AnswererRejectsAudioAndVideoSections) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    // Don't give the callee any tracks, and set offer_to_receive_X to 0, so it
    // will reject both audio and video m= sections.
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    options.offer_to_receive_video = 0;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    fixture.callee()->SetRemoteOfferHandler([&] {
      // Stopping all transceivers will cause all media sections to be rejected.
      for (const auto& transceiver :
           fixture.callee()->pc()->GetTransceivers()) {
        transceiver->Stop();
      }
    });
  }
  // Do offer/answer and wait for stable signaling state.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Sanity check that the callee's description has rejected m= sections.
  ASSERT_NE(nullptr, fixture.callee()->pc()->local_description());
  const ContentInfo* callee_audio_content = GetFirstAudioContent(
      fixture.callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_audio_content);
  EXPECT_TRUE(callee_audio_content->rejected);
  const ContentInfo* callee_video_content = GetFirstVideoContent(
      fixture.callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
}

// This test sets up an audio and video call between two parties. After the
// call runs for a while, the caller sends an updated offer with video being
// rejected. Once the re-negotiation is done, the video flow should stop and
// the audio flow should continue.
TEST_P(PeerConnectionIntegrationTest, VideoRejectedInSubsequentOffer) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
  // Renegotiate, rejecting the video m= section.
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    fixture.caller()->SetGeneratedSdpMunger(
        [](cricket::SessionDescription* description) {
          for (cricket::ContentInfo& content : description->contents()) {
            if (cricket::IsVideoContent(&content)) {
              content.rejected = true;
            }
          }
        });
  } else {
    fixture.caller()
        ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
        ->Stop();
  }
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kMaxWaitForActivationMs);

  // Sanity check that the caller's description has a rejected video section.
  ASSERT_NE(nullptr, fixture.caller()->pc()->local_description());
  const ContentInfo* caller_video_content = GetFirstVideoContent(
      fixture.caller()->pc()->local_description()->description());
  ASSERT_NE(nullptr, caller_video_content);
  EXPECT_TRUE(caller_video_content->rejected);
  // Wait for some additional audio frames to be received.
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudio();
    media_expectations.ExpectNoVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
}

// Do one offer/answer with audio, another that disables it (rejecting the m=
// section), and another that re-enables it. Regression test for:
// bugs.webrtc.org/6023
TEST_F(PeerConnectionIntegrationTestPlanB, EnableAudioAfterRejecting) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Add audio track, do normal offer/answer.
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      fixture.caller()->CreateLocalAudioTrack();
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender =
      fixture.caller()->pc()->AddTrack(track, {"stream"}).MoveValue();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Remove audio track, and set offer_to_receive_audio to false to cause the
  // m= section to be completely disabled, not just "recvonly".
  fixture.caller()->pc()->RemoveTrack(sender);
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Add the audio track again, expecting negotiation to succeed and frames to
  // flow.
  sender = fixture.caller()->pc()->AddTrack(track, {"stream"}).MoveValue();
  options.offer_to_receive_audio = 1;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  EXPECT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Basic end-to-end test, but without SSRC/MSID signaling. This functionality
// is needed to support legacy endpoints.
// TODO(deadbeef): When we support the MID extension and demuxing on MID, also
// add a test for an end-to-end test without MID signaling either (basically,
// the minimum acceptable SDP).
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithoutSsrcOrMsidSignaling) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Add audio and video, testing that packets can be demuxed on payload type.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  // Remove SSRCs and MSIDs from the received offer SDP.
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Basic end-to-end test, without SSRC signaling. This means that the track
// was created properly and frames are delivered when the MSIDs are communicated
// with a=msid lines and no a=ssrc lines.
TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithoutSsrcSignaling) {
  const char kStreamId[] = "streamId";
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Add just audio tracks.
  fixture.caller()->AddTrack(fixture.caller()->CreateLocalAudioTrack(),
                             {kStreamId});
  fixture.callee()->AddAudioTrack();

  // Remove SSRCs from the received offer SDP.
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndKeepMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Tests that video flows between multiple video tracks when SSRCs are not
// signaled. This exercises the MID RTP header extension which is needed to
// demux the incoming video tracks.
TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithTwoVideoTracksAndNoSignaledSsrc) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddVideoTrack();
  fixture.caller()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();

  fixture.caller()->SetReceivedSdpMunger(&RemoveSsrcsAndKeepMsids);
  fixture.callee()->SetReceivedSdpMunger(&RemoveSsrcsAndKeepMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(2u, fixture.caller()->pc()->GetReceivers().size());
  ASSERT_EQ(2u, fixture.callee()->pc()->GetReceivers().size());

  // Expect video to be received in both directions on both tracks.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  EXPECT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan, NoStreamsMsidLinePresent) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  auto callee_receivers = fixture.callee()->pc()->GetReceivers();
  ASSERT_EQ(2u, callee_receivers.size());
  EXPECT_TRUE(callee_receivers[0]->stream_ids().empty());
  EXPECT_TRUE(callee_receivers[1]->stream_ids().empty());
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan, NoStreamsMsidLineMissing) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  fixture.caller()->AddVideoTrack();
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  auto callee_receivers = fixture.callee()->pc()->GetReceivers();
  ASSERT_EQ(2u, callee_receivers.size());
  ASSERT_EQ(1u, callee_receivers[0]->stream_ids().size());
  ASSERT_EQ(1u, callee_receivers[1]->stream_ids().size());
  EXPECT_EQ(callee_receivers[0]->stream_ids()[0],
            callee_receivers[1]->stream_ids()[0]);
  EXPECT_EQ(callee_receivers[0]->streams()[0],
            callee_receivers[1]->streams()[0]);
}

// Test that if two video tracks are sent (from caller to callee, in this test),
// they're transmitted correctly end-to-end.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithTwoVideoTracks) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Add one audio/video stream, and one video-only stream.
  fixture.caller()->AddAudioVideoTracks();
  fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(3u, fixture.callee()->pc()->GetReceivers().size());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

static void MakeSpecCompliantMaxBundleOffer(cricket::SessionDescription* desc) {
  bool first = true;
  for (cricket::ContentInfo& content : desc->contents()) {
    if (first) {
      first = false;
      continue;
    }
    content.bundle_only = true;
  }
  first = true;
  for (cricket::TransportInfo& transport : desc->transport_infos()) {
    if (first) {
      first = false;
      continue;
    }
    transport.description.ice_ufrag.clear();
    transport.description.ice_pwd.clear();
    transport.description.connection_role = cricket::CONNECTIONROLE_NONE;
    transport.description.identity_fingerprint.reset(nullptr);
  }
}

// Test that if applying a true "max bundle" offer, which uses ports of 0,
// "a=bundle-only", omitting "a=fingerprint", "a=setup", "a=ice-ufrag" and
// "a=ice-pwd" for all but the audio "m=" section, negotiation still completes
// successfully and media flows.
// TODO(deadbeef): Update this test to also omit "a=rtcp-mux", once that works.
// TODO(deadbeef): Won't need this test once we start generating actual
// standards-compliant SDP.
TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallWithSpecCompliantMaxBundleOffer) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  // Do the equivalent of setting the port to 0, adding a=bundle-only, and
  // removing a=ice-ufrag, a=ice-pwd, a=fingerprint and a=setup from all
  // but the first m= section.
  fixture.callee()->SetReceivedSdpMunger(MakeSpecCompliantMaxBundleOffer);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that we can receive the audio output level from a remote audio track.
// TODO(deadbeef): Use a fake audio source and verify that the output level is
// exactly what the source on the other side was configured with.
TEST_P(PeerConnectionIntegrationTest, GetAudioOutputLevelStatsWithOldStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Just add an audio track.
  fixture.caller()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Get the audio output level stats. Note that the level is not available
  // until an RTCP packet has been received.
  EXPECT_TRUE_WAIT(fixture.callee()->OldGetStats()->AudioOutputLevel() > 0,
                   kMaxWaitForFramesMs);
}

// Test that an audio input level is reported.
// TODO(deadbeef): Use a fake audio source and verify that the input level is
// exactly what the source was configured with.
TEST_P(PeerConnectionIntegrationTest, GetAudioInputLevelStatsWithOldStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Just add an audio track.
  fixture.caller()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Get the audio input level stats. The level should be available very
  // soon after the test starts.
  EXPECT_TRUE_WAIT(fixture.caller()->OldGetStats()->AudioInputLevel() > 0,
                   kMaxWaitForStatsMs);
}

// Test that we can get incoming byte counts from both audio and video tracks.
TEST_P(PeerConnectionIntegrationTest, GetBytesReceivedStatsWithOldStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  // Do offer/answer, wait for the callee to receive some frames.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // Get a handle to the remote tracks created, so they can be used as GetStats
  // filters.
  for (const auto& receiver : fixture.callee()->pc()->GetReceivers()) {
    // We received frames, so we definitely should have nonzero "received bytes"
    // stats at this point.
    EXPECT_GT(fixture.callee()
                  ->OldGetStatsForTrack(receiver->track())
                  ->BytesReceived(),
              0);
  }
}

// Test that we can get outgoing byte counts from both audio and video tracks.
TEST_P(PeerConnectionIntegrationTest, GetBytesSentStatsWithOldStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  auto audio_track = fixture.caller()->CreateLocalAudioTrack();
  auto video_track = fixture.caller()->CreateLocalVideoTrack();
  fixture.caller()->AddTrack(audio_track);
  fixture.caller()->AddTrack(video_track);
  // Do offer/answer, wait for the callee to receive some frames.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // The callee received frames, so we definitely should have nonzero "sent
  // bytes" stats at this point.
  EXPECT_GT(fixture.caller()->OldGetStatsForTrack(audio_track)->BytesSent(), 0);
  EXPECT_GT(fixture.caller()->OldGetStatsForTrack(video_track)->BytesSent(), 0);
}

// Test that we can get capture start ntp time.
TEST_P(PeerConnectionIntegrationTest, GetCaptureStartNtpTimeWithOldStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();

  fixture.callee()->AddAudioTrack();

  // Do offer/answer, wait for the callee to receive some frames.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Get the remote audio track created on the receiver, so they can be used as
  // GetStats filters.
  auto receivers = fixture.callee()->pc()->GetReceivers();
  ASSERT_EQ(1u, receivers.size());
  auto remote_audio_track = receivers[0]->track();

  // Get the audio output level stats. Note that the level is not available
  // until an RTCP packet has been received.
  EXPECT_TRUE_WAIT(fixture.callee()
                           ->OldGetStatsForTrack(remote_audio_track)
                           ->CaptureStartNtpTime() > 0,
                   2 * kMaxWaitForFramesMs);
}

// Test that the track ID is associated with all local and remote SSRC stats
// using the old GetStats() and more than 1 audio and more than 1 video track.
// This is a regression test for crbug.com/906988
TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       OldGetStatsAssociatesTrackIdForManyMediaSections) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  auto audio_sender_1 = fixture.caller()->AddAudioTrack();
  auto video_sender_1 = fixture.caller()->AddVideoTrack();
  auto audio_sender_2 = fixture.caller()->AddAudioTrack();
  auto video_sender_2 = fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE_WAIT(fixture.ExpectNewFrames(media_expectations),
                   kDefaultTimeout);

  std::vector<std::string> track_ids = {
      audio_sender_1->track()->id(), video_sender_1->track()->id(),
      audio_sender_2->track()->id(), video_sender_2->track()->id()};

  auto caller_stats = fixture.caller()->OldGetStats();
  EXPECT_THAT(caller_stats->TrackIds(), UnorderedElementsAreArray(track_ids));
  auto callee_stats = fixture.callee()->OldGetStats();
  EXPECT_THAT(callee_stats->TrackIds(), UnorderedElementsAreArray(track_ids));
}

// Test that the new GetStats() returns stats for all outgoing/incoming streams
// with the correct track IDs if there are more than one audio and more than one
// video senders/receivers.
TEST_P(PeerConnectionIntegrationTest, NewGetStatsManyAudioAndManyVideoStreams) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  auto audio_sender_1 = fixture.caller()->AddAudioTrack();
  auto video_sender_1 = fixture.caller()->AddVideoTrack();
  auto audio_sender_2 = fixture.caller()->AddAudioTrack();
  auto video_sender_2 = fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE_WAIT(fixture.ExpectNewFrames(media_expectations),
                   kDefaultTimeout);

  std::vector<std::string> track_ids = {
      audio_sender_1->track()->id(), video_sender_1->track()->id(),
      audio_sender_2->track()->id(), video_sender_2->track()->id()};

  rtc::scoped_refptr<const webrtc::RTCStatsReport> caller_report =
      fixture.caller()->NewGetStats();
  ASSERT_TRUE(caller_report);
  auto outbound_stream_stats =
      caller_report->GetStatsOfType<webrtc::RTCOutboundRTPStreamStats>();
  ASSERT_EQ(4u, outbound_stream_stats.size());
  std::vector<std::string> outbound_track_ids;
  for (const auto& stat : outbound_stream_stats) {
    ASSERT_TRUE(stat->bytes_sent.is_defined());
    EXPECT_LT(0u, *stat->bytes_sent);
    ASSERT_TRUE(stat->track_id.is_defined());
    const auto* track_stat =
        caller_report->GetAs<webrtc::RTCMediaStreamTrackStats>(*stat->track_id);
    ASSERT_TRUE(track_stat);
    outbound_track_ids.push_back(*track_stat->track_identifier);
  }
  EXPECT_THAT(outbound_track_ids, UnorderedElementsAreArray(track_ids));

  rtc::scoped_refptr<const webrtc::RTCStatsReport> callee_report =
      fixture.callee()->NewGetStats();
  ASSERT_TRUE(callee_report);
  auto inbound_stream_stats =
      callee_report->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>();
  ASSERT_EQ(4u, inbound_stream_stats.size());
  std::vector<std::string> inbound_track_ids;
  for (const auto& stat : inbound_stream_stats) {
    ASSERT_TRUE(stat->bytes_received.is_defined());
    EXPECT_LT(0u, *stat->bytes_received);
    ASSERT_TRUE(stat->track_id.is_defined());
    const auto* track_stat =
        callee_report->GetAs<webrtc::RTCMediaStreamTrackStats>(*stat->track_id);
    ASSERT_TRUE(track_stat);
    inbound_track_ids.push_back(*track_stat->track_identifier);
  }
  EXPECT_THAT(inbound_track_ids, UnorderedElementsAreArray(track_ids));
}

// Test that we can get stats (using the new stats implementation) for
// unsignaled streams. Meaning when SSRCs/MSIDs aren't signaled explicitly in
// SDP.
TEST_P(PeerConnectionIntegrationTest,
       GetStatsForUnsignaledStreamWithNewStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  // Remove SSRCs and MSIDs from the received offer SDP.
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // We received a frame, so we should have nonzero "bytes received" stats for
  // the unsignaled stream, if stats are working for it.
  rtc::scoped_refptr<const webrtc::RTCStatsReport> report =
      fixture.callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  auto inbound_stream_stats =
      report->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>();
  ASSERT_EQ(1U, inbound_stream_stats.size());
  ASSERT_TRUE(inbound_stream_stats[0]->bytes_received.is_defined());
  ASSERT_GT(*inbound_stream_stats[0]->bytes_received, 0U);
  ASSERT_TRUE(inbound_stream_stats[0]->track_id.is_defined());
}

// Same as above but for the legacy stats implementation.
TEST_P(PeerConnectionIntegrationTest,
       GetStatsForUnsignaledStreamWithOldStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  // Remove SSRCs and MSIDs from the received offer SDP.
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Note that, since the old stats implementation associates SSRCs with tracks
  // using SDP, when SSRCs aren't signaled in SDP these stats won't have an
  // associated track ID. So we can't use the track "selector" argument.
  //
  // Also, we use "EXPECT_TRUE_WAIT" because the stats collector may decide to
  // return cached stats if not enough time has passed since the last update.
  EXPECT_TRUE_WAIT(fixture.callee()->OldGetStats()->BytesReceived() > 0,
                   kDefaultTimeout);
}

// Test that we can successfully get the media related stats (audio level
// etc.) for the unsignaled stream.
TEST_P(PeerConnectionIntegrationTest,
       GetMediaStatsForUnsignaledStreamWithNewStatsApi) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  // Remove SSRCs and MSIDs from the received offer SDP.
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  media_expectations.CalleeExpectsSomeVideo(1);
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  rtc::scoped_refptr<const webrtc::RTCStatsReport> report =
      fixture.callee()->NewGetStats();
  ASSERT_NE(nullptr, report);

  auto media_stats = report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  auto audio_index = FindFirstMediaStatsIndexByKind("audio", media_stats);
  ASSERT_GE(audio_index, 0);
  EXPECT_TRUE(media_stats[audio_index]->audio_level.is_defined());
}

// Helper for test below.
void ModifySsrcs(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    for (StreamParams& stream :
         content.media_description()->mutable_streams()) {
      for (uint32_t& ssrc : stream.ssrcs) {
        ssrc = rtc::CreateRandomId();
      }
    }
  }
}

// Test that the "RTCMediaSteamTrackStats"  object is updated correctly when
// SSRCs are unsignaled, and the SSRC of the received (audio) stream changes.
// This should result in two "RTCInboundRTPStreamStats", but only one
// "RTCMediaStreamTrackStats", whose counters go up continuously rather than
// being reset to 0 once the SSRC change occurs.
//
// Regression test for this bug:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=8158
//
// The bug causes the track stats to only represent one of the two streams:
// whichever one has the higher SSRC. So with this bug, there was a 50% chance
// that the track stat counters would reset to 0 when the new stream is
// received, and a 50% chance that they'll stop updating (while
// "concealed_samples" continues increasing, due to silence being generated for
// the inactive stream).
TEST_P(PeerConnectionIntegrationTest,
       TrackStatsUpdatedCorrectlyWhenUnsignaledSsrcChanges) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  // Remove SSRCs and MSIDs from the received offer SDP, simulating an endpoint
  // that doesn't signal SSRCs (from the callee's perspective).
  fixture.callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for 50 audio frames (500ms of audio) to be received by the callee.
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio(50);
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
  // Some audio frames were received, so we should have nonzero "samples
  // received" for the track.
  rtc::scoped_refptr<const webrtc::RTCStatsReport> report =
      fixture.callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  auto track_stats = report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, track_stats.size());
  ASSERT_TRUE(track_stats[0]->total_samples_received.is_defined());
  ASSERT_GT(*track_stats[0]->total_samples_received, 0U);
  // uint64_t prev_samples_received = *track_stats[0]->total_samples_received;

  // Create a new offer and munge it to cause the caller to use a new SSRC.
  fixture.caller()->SetGeneratedSdpMunger(ModifySsrcs);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for 25 more audio frames (250ms of audio) to be received, from the new
  // SSRC.
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio(25);
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }

  report = fixture.callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  track_stats = report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, track_stats.size());
  ASSERT_TRUE(track_stats[0]->total_samples_received.is_defined());
  // The "total samples received" stat should only be greater than it was
  // before.
  // TODO(deadbeef): Uncomment this assertion once the bug is completely fixed.
  // Right now, the new SSRC will cause the counters to reset to 0.
  // EXPECT_GT(*track_stats[0]->total_samples_received, prev_samples_received);

  // Additionally, the percentage of concealed samples (samples generated to
  // conceal packet loss) should be less than 50%. If it's greater, that's a
  // good sign that we're seeing stats from the old stream that's no longer
  // receiving packets, and is generating concealed samples of silence.
  constexpr double kAcceptableConcealedSamplesPercentage = 0.50;
  ASSERT_TRUE(track_stats[0]->concealed_samples.is_defined());
  EXPECT_LT(*track_stats[0]->concealed_samples,
            *track_stats[0]->total_samples_received *
                kAcceptableConcealedSamplesPercentage);

  // Also ensure that we have two "RTCInboundRTPStreamStats" as expected, as a
  // sanity check that the SSRC really changed.
  // TODO(deadbeef): This isn't working right now, because we're not returning
  // *any* stats for the inactive stream. Uncomment when the bug is completely
  // fixed.
  // auto inbound_stream_stats =
  //     report->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>();
  // ASSERT_EQ(2U, inbound_stream_stats.size());
}

// Test that DTLS 1.0 is used if both sides only support DTLS 1.0.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithDtls10) {
  PeerConnectionFactory::Options dtls_10_options;
  dtls_10_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;

  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithOptions(dtls_10_options,
                                                              dtls_10_options));
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test getting cipher stats and UMA metrics when DTLS 1.0 is negotiated.
TEST_P(PeerConnectionIntegrationTest, Dtls10CipherStatsAndUmaMetrics) {
  PeerConnectionFactory::Options dtls_10_options;
  dtls_10_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;

  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithOptions(dtls_10_options,
                                                              dtls_10_options));
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(
      rtc::SSLStreamAdapter::IsAcceptableCipher(
          fixture.caller()->OldGetStats()->DtlsCipher(), rtc::KT_DEFAULT),
      kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 fixture.caller()->OldGetStats()->SrtpCipher(),
                 kDefaultTimeout);
  // TODO(bugs.webrtc.org/9456): Fix it.
  EXPECT_EQ(1, webrtc::metrics::NumEvents(
                   "WebRTC.PeerConnection.SrtpCryptoSuite.Audio",
                   kDefaultSrtpCryptoSuite));
}

// Test getting cipher stats and UMA metrics when DTLS 1.2 is negotiated.
TEST_P(PeerConnectionIntegrationTest, Dtls12CipherStatsAndUmaMetrics) {
  PeerConnectionFactory::Options dtls_12_options;
  dtls_12_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;

  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithOptions(dtls_12_options,
                                                              dtls_12_options));
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(
      rtc::SSLStreamAdapter::IsAcceptableCipher(
          fixture.caller()->OldGetStats()->DtlsCipher(), rtc::KT_DEFAULT),
      kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 fixture.caller()->OldGetStats()->SrtpCipher(),
                 kDefaultTimeout);
  // TODO(bugs.webrtc.org/9456): Fix it.
  EXPECT_EQ(1, webrtc::metrics::NumEvents(
                   "WebRTC.PeerConnection.SrtpCryptoSuite.Audio",
                   kDefaultSrtpCryptoSuite));
}

// Test that DTLS 1.0 can be used if the caller supports DTLS 1.2 and the
// callee only supports 1.0.
TEST_P(PeerConnectionIntegrationTest, CallerDtls12ToCalleeDtls10) {
  PeerConnectionFactory::Options caller_options;
  caller_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  PeerConnectionFactory::Options callee_options;
  callee_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithOptions(caller_options,
                                                              callee_options));
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that DTLS 1.0 can be used if the caller only supports DTLS 1.0 and the
// callee supports 1.2.
TEST_P(PeerConnectionIntegrationTest, CallerDtls10ToCalleeDtls12) {
  PeerConnectionFactory::Options caller_options;
  caller_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  PeerConnectionFactory::Options callee_options;
  callee_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithOptions(caller_options,
                                                              callee_options));
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// The three tests below verify that "enable_aes128_sha1_32_crypto_cipher"
// works as expected; the cipher should only be used if enabled by both sides.
TEST_P(PeerConnectionIntegrationTest,
       Aes128Sha1_32_CipherNotUsedWhenOnlyCallerSupported) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  PeerConnectionFactory::Options caller_options;
  caller_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  PeerConnectionFactory::Options callee_options;
  callee_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher =
      false;
  int expected_cipher_suite = rtc::SRTP_AES128_CM_SHA1_80;
  fixture.TestNegotiatedCipherSuite(caller_options, callee_options,
                                    expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest,
       Aes128Sha1_32_CipherNotUsedWhenOnlyCalleeSupported) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  PeerConnectionFactory::Options caller_options;
  caller_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher =
      false;
  PeerConnectionFactory::Options callee_options;
  callee_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  int expected_cipher_suite = rtc::SRTP_AES128_CM_SHA1_80;
  fixture.TestNegotiatedCipherSuite(caller_options, callee_options,
                                    expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest, Aes128Sha1_32_CipherUsedWhenSupported) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  PeerConnectionFactory::Options caller_options;
  caller_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  PeerConnectionFactory::Options callee_options;
  callee_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  int expected_cipher_suite = rtc::SRTP_AES128_CM_SHA1_32;
  fixture.TestNegotiatedCipherSuite(caller_options, callee_options,
                                    expected_cipher_suite);
}

// Test that a non-GCM cipher is used if both sides only support non-GCM.
TEST_P(PeerConnectionIntegrationTest, NonGcmCipherUsedWhenGcmNotSupported) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  bool local_gcm_enabled = false;
  bool remote_gcm_enabled = false;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  fixture.TestGcmNegotiationUsesCipherSuite(
      local_gcm_enabled, remote_gcm_enabled, expected_cipher_suite);
}

// Test that a GCM cipher is used if both ends support it.
TEST_P(PeerConnectionIntegrationTest, GcmCipherUsedWhenGcmSupported) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  bool local_gcm_enabled = true;
  bool remote_gcm_enabled = true;
  int expected_cipher_suite = kDefaultSrtpCryptoSuiteGcm;
  fixture.TestGcmNegotiationUsesCipherSuite(
      local_gcm_enabled, remote_gcm_enabled, expected_cipher_suite);
}

// Test that GCM isn't used if only the offerer supports it.
TEST_P(PeerConnectionIntegrationTest,
       NonGcmCipherUsedWhenOnlyCallerSupportsGcm) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  bool local_gcm_enabled = true;
  bool remote_gcm_enabled = false;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  fixture.TestGcmNegotiationUsesCipherSuite(
      local_gcm_enabled, remote_gcm_enabled, expected_cipher_suite);
}

// Test that GCM isn't used if only the answerer supports it.
TEST_P(PeerConnectionIntegrationTest,
       NonGcmCipherUsedWhenOnlyCalleeSupportsGcm) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  bool local_gcm_enabled = false;
  bool remote_gcm_enabled = true;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  fixture.TestGcmNegotiationUsesCipherSuite(
      local_gcm_enabled, remote_gcm_enabled, expected_cipher_suite);
}

// Verify that media can be transmitted end-to-end when GCM crypto suites are
// enabled. Note that the above tests, such as GcmCipherUsedWhenGcmSupported,
// only verify that a GCM cipher is negotiated, and not necessarily that SRTP
// works with it.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithGcmCipher) {
  PeerConnectionFactory::Options gcm_options;
  gcm_options.crypto_options.srtp.enable_gcm_crypto_suites = true;

  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithOptions(gcm_options,
                                                              gcm_options));
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This test sets up a call between two parties with audio, video and an RTP
// data channel.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithRtpDataChannel) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  fixture.ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Ensure the existence of the RTP data channel didn't impede audio/video.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_NE(nullptr, fixture.callee()->data_channel());
  EXPECT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  fixture.SendRtpDataWithRetries(fixture.caller()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, fixture.callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  fixture.SendRtpDataWithRetries(fixture.callee()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, fixture.caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Ensure that an RTP data channel is signaled as closed for the caller when
// the callee rejects it in a subsequent offer.
TEST_P(PeerConnectionIntegrationTest,
       RtpDataChannelSignaledClosedInCalleeOffer) {
  // Same procedure as above test.
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_NE(nullptr, fixture.callee()->data_channel());
  ASSERT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Close the data channel on the callee, and do an updated offer/answer.
  fixture.callee()->data_channel()->Close();
  fixture.callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  EXPECT_FALSE(fixture.caller()->data_observer()->IsOpen());
  EXPECT_FALSE(fixture.callee()->data_observer()->IsOpen());
}

// Tests that data is buffered in an RTP data channel until an observer is
// registered for it.
//
// NOTE: RTP data channels can receive data before the underlying
// transport has detected that a channel is writable and thus data can be
// received before the data channel state changes to open. That is hard to test
// but the same buffering is expected to be used in that case.
TEST_P(PeerConnectionIntegrationTest,
       DataBufferedUntilRtpDataChannelObserverRegistered) {
  // Use fake clock and simulated network delay so that we predictably can wait
  // until an SCTP message has been delivered without "sleep()"ing.
  rtc::ScopedFakeClock fake_clock;
  // Some things use a time of "0" as a special value, so we need to start out
  // the fake clock at a nonzero time.
  // TODO(deadbeef): Fix this.
  fake_clock.AdvanceTime(webrtc::TimeDelta::seconds(1));

  // The fixture is created after clock to ensure that PeerConnections are
  // destroyed before ScopedFakeClock. If this is not done a DCHECK can be hit
  // in ports.cc, because a large negative number is calculated for the rtt due
  // to the global clock changing.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  fixture.virtual_socket_server()->set_delay_mean(5);  // 5 ms per hop.
  fixture.virtual_socket_server()->UpdateDelayDistribution();

  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE(fixture.caller()->data_channel() != nullptr);
  ASSERT_TRUE_SIMULATED_WAIT(fixture.callee()->data_channel() != nullptr,
                             kDefaultTimeout, fake_clock);
  ASSERT_TRUE_SIMULATED_WAIT(fixture.caller()->data_observer()->IsOpen(),
                             kDefaultTimeout, fake_clock);
  ASSERT_EQ_SIMULATED_WAIT(DataChannelInterface::kOpen,
                           fixture.callee()->data_channel()->state(),
                           kDefaultTimeout, fake_clock);

  // Unregister the observer which is normally automatically registered.
  fixture.callee()->data_channel()->UnregisterObserver();
  // Send data and advance fake clock until it should have been received.
  std::string data = "hello world";
  fixture.caller()->data_channel()->Send(DataBuffer(data));
  SIMULATED_WAIT(false, 50, fake_clock);

  // Attach data channel and expect data to be received immediately. Note that
  // EXPECT_EQ_WAIT is used, such that the simulated clock is not advanced any
  // further, but data can be received even if the callback is asynchronous.
  MockDataChannelObserver new_observer(fixture.callee()->data_channel());
  EXPECT_EQ_SIMULATED_WAIT(data, new_observer.last_message(), kDefaultTimeout,
                           fake_clock);
}

// This test sets up a call between two parties with audio, video and but only
// the caller client supports RTP data channels.
TEST_P(PeerConnectionIntegrationTest, RtpDataChannelsRejectedByCallee) {
  PeerConnectionInterface::RTCConfiguration rtc_config_1;
  rtc_config_1.enable_rtp_data_channel = true;
  // Must disable DTLS to make negotiation succeed.
  rtc_config_1.enable_dtls_srtp = false;
  PeerConnectionInterface::RTCConfiguration rtc_config_2;
  rtc_config_2.enable_dtls_srtp = false;
  rtc_config_2.enable_dtls_srtp = false;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(rtc_config_1,
                                                             rtc_config_2));
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();
  ASSERT_TRUE(fixture.caller()->data_channel() != nullptr);
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // The caller should still have a data channel, but it should be closed, and
  // one should ever have been created for the callee.
  EXPECT_TRUE(fixture.caller()->data_channel() != nullptr);
  EXPECT_FALSE(fixture.caller()->data_observer()->IsOpen());
  EXPECT_EQ(nullptr, fixture.callee()->data_channel());
}

// This test sets up a call between two parties with audio, and video. When
// audio and video is setup and flowing, an RTP data channel is negotiated.
TEST_P(PeerConnectionIntegrationTest, AddRtpDataChannelInSubsequentOffer) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  fixture.ConnectFakeSignaling();
  // Do initial offer/answer with audio/video.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Create data channel and do new offer and answer.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_NE(nullptr, fixture.callee()->data_channel());
  EXPECT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  fixture.SendRtpDataWithRetries(fixture.caller()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, fixture.callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  fixture.SendRtpDataWithRetries(fixture.callee()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, fixture.caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#ifdef HAVE_SCTP

// This test sets up a call between two parties with audio, video and an SCTP
// data channel.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithSctpDataChannel) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Ensure the existence of the SCTP data channel didn't impede audio/video.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  fixture.caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  fixture.callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Ensure that when the callee closes an SCTP data channel, the closing
// procedure results in the data channel being closed for the caller as well.
TEST_P(PeerConnectionIntegrationTest, CalleeClosesSctpDataChannel) {
  // Same procedure as above test.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Close the data channel on the callee side, and wait for it to reach the
  // "closed" state on both sides.
  fixture.callee()->data_channel()->Close();
  EXPECT_TRUE_WAIT(!fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(!fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest, SctpDataChannelConfigSentToOtherSide) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  webrtc::DataChannelInit init;
  init.id = 53;
  init.maxRetransmits = 52;
  fixture.caller()->CreateDataChannel("data-channel", &init);
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  // Since "negotiated" is false, the "id" parameter should be ignored.
  EXPECT_NE(init.id, fixture.callee()->data_channel()->id());
  EXPECT_EQ("data-channel", fixture.callee()->data_channel()->label());
  EXPECT_EQ(init.maxRetransmits,
            fixture.callee()->data_channel()->maxRetransmits());
  EXPECT_FALSE(fixture.callee()->data_channel()->negotiated());
}

// Test usrsctp's ability to process unordered data stream, where data actually
// arrives out of order using simulated delays. Previously there have been some
// bugs in this area.
TEST_P(PeerConnectionIntegrationTest, StressTestUnorderedSctpDataChannel) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  // Introduce random network delays.
  // Otherwise it's not a true "unordered" test.
  fixture.virtual_socket_server()->set_delay_mean(20);
  fixture.virtual_socket_server()->set_delay_stddev(5);
  fixture.virtual_socket_server()->UpdateDelayDistribution();
  // Normal procedure, but with unordered data channel config.
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  webrtc::DataChannelInit init;
  init.ordered = false;
  fixture.caller()->CreateDataChannel(&init);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  static constexpr int kNumMessages = 100;
  // Deliberately chosen to be larger than the MTU so messages get fragmented.
  static constexpr size_t kMaxMessageSize = 4096;
  // Create and send random messages.
  std::vector<std::string> sent_messages;
  for (int i = 0; i < kNumMessages; ++i) {
    size_t length =
        (rand() % kMaxMessageSize) + 1;  // NOLINT (rand_r instead of rand)
    std::string message;
    ASSERT_TRUE(rtc::CreateRandomString(length, &message));
    fixture.caller()->data_channel()->Send(DataBuffer(message));
    fixture.callee()->data_channel()->Send(DataBuffer(message));
    sent_messages.push_back(message);
  }

  // Wait for all messages to be received.
  EXPECT_EQ_WAIT(rtc::checked_cast<size_t>(kNumMessages),
                 fixture.caller()->data_observer()->received_message_count(),
                 kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::checked_cast<size_t>(kNumMessages),
                 fixture.callee()->data_observer()->received_message_count(),
                 kDefaultTimeout);

  // Sort and compare to make sure none of the messages were corrupted.
  std::vector<std::string> caller_received_messages =
      fixture.caller()->data_observer()->messages();
  std::vector<std::string> callee_received_messages =
      fixture.callee()->data_observer()->messages();
  absl::c_sort(sent_messages);
  absl::c_sort(caller_received_messages);
  absl::c_sort(callee_received_messages);
  EXPECT_EQ(sent_messages, caller_received_messages);
  EXPECT_EQ(sent_messages, callee_received_messages);
}

// This test sets up a call between two parties with audio, and video. When
// audio and video are setup and flowing, an SCTP data channel is negotiated.
TEST_P(PeerConnectionIntegrationTest, AddSctpDataChannelInSubsequentOffer) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Do initial offer/answer with audio/video.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Create data channel and do new offer and answer.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  fixture.caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  fixture.callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Set up a connection initially just using SCTP data channels, later upgrading
// to audio/video, ensuring frames are received end-to-end. Effectively the
// inverse of the test above.
// This was broken in M57; see https://crbug.com/711243
TEST_P(PeerConnectionIntegrationTest, SctpDataChannelToAudioVideoUpgrade) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Do initial offer/answer with just data channel.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait until data can be sent over the data channel.
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Do subsequent offer/answer with two-way audio and video. Audio and video
  // should end up bundled on the DTLS/ICE transport already used for data.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

static void MakeSpecCompliantSctpOffer(cricket::SessionDescription* desc) {
  cricket::SctpDataContentDescription* dcd_offer =
      GetFirstSctpDataContentDescription(desc);
  ASSERT_TRUE(dcd_offer);
  dcd_offer->set_use_sctpmap(false);
  dcd_offer->set_protocol("UDP/DTLS/SCTP");
}

// Test that the data channel works when a spec-compliant SCTP m= section is
// offered (using "a=sctp-port" instead of "a=sctpmap", and using
// "UDP/DTLS/SCTP" as the protocol).
TEST_P(PeerConnectionIntegrationTest,
       DataChannelWorksWhenSpecCompliantSctpOfferReceived) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();
  fixture.caller()->SetGeneratedSdpMunger(MakeSpecCompliantSctpOffer);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  fixture.caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  fixture.callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#endif  // HAVE_SCTP

// This test sets up a call between two parties with a media transport data
// channel.
TEST_P(PeerConnectionIntegrationTest, MediaTransportDataChannelEndToEnd) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  rtc_config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  rtc_config.use_media_transport_for_data_channels = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  // Expect that data channel created on caller side will show up for callee as
  // well.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  fixture.caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  fixture.callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, fixture.caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Ensure that when the callee closes a media transport data channel, the
// closing procedure results in the data channel being closed for the caller
// as well.
TEST_P(PeerConnectionIntegrationTest, MediaTransportDataChannelCalleeCloses) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.use_media_transport_for_data_channels = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  // Create a data channel on the caller and signal it to the callee.
  fixture.caller()->CreateDataChannel();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  // Data channels exist and open on both ends of the connection.
  ASSERT_NE(nullptr, fixture.caller()->data_channel());
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);

  // Close the data channel on the callee side, and wait for it to reach the
  // "closed" state on both sides.
  fixture.callee()->data_channel()->Close();
  EXPECT_TRUE_WAIT(!fixture.caller()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(!fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest,
       MediaTransportDataChannelConfigSentToOtherSide) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.use_media_transport_for_data_channels = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  // Create a data channel with a non-default configuration and signal it to the
  // callee.
  webrtc::DataChannelInit init;
  init.id = 53;
  init.maxRetransmits = 52;
  fixture.caller()->CreateDataChannel("data-channel", &init);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  // Ensure that the data channel exists on the callee with the correct
  // configuration.
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel() != nullptr,
                   kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_observer()->IsOpen(),
                   kDefaultTimeout);
  // Since "negotiate" is false, the "id" parameter is ignored.
  EXPECT_NE(init.id, fixture.callee()->data_channel()->id());
  EXPECT_EQ("data-channel", fixture.callee()->data_channel()->label());
  EXPECT_EQ(init.maxRetransmits,
            fixture.callee()->data_channel()->maxRetransmits());
  EXPECT_FALSE(fixture.callee()->data_channel()->negotiated());
}

TEST_P(PeerConnectionIntegrationTest, MediaTransportOfferUpgrade) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  rtc_config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  rtc_config.use_media_transport = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  // Do initial offer/answer with just a video track.
  fixture.caller()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  // Now add an audio track and do another offer/answer.
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure both audio and video frames are received end-to-end.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // The second offer should not have generated another media transport.
  // Media transport was kept alive, and was not recreated.
  EXPECT_EQ(
      1, fixture.loopback_media_transports()->first_factory_transport_count());
  EXPECT_EQ(
      1, fixture.loopback_media_transports()->second_factory_transport_count());
}

TEST_P(PeerConnectionIntegrationTest, MediaTransportOfferUpgradeOnTheCallee) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  rtc_config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  rtc_config.use_media_transport = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  // Do initial offer/answer with just a video track.
  fixture.caller()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  // Now add an audio track and do another offer/answer.
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure both audio and video frames are received end-to-end.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  // The second offer should not have generated another media transport.
  // Media transport was kept alive, and was not recreated.
  EXPECT_EQ(
      1, fixture.loopback_media_transports()->first_factory_transport_count());
  EXPECT_EQ(
      1, fixture.loopback_media_transports()->second_factory_transport_count());
}

TEST_P(PeerConnectionIntegrationTest, MediaTransportBidirectionalAudio) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  rtc_config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  rtc_config.use_media_transport = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  // Start offer/answer exchange and wait for it to complete.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  webrtc::MediaTransportPair::Stats first_stats =
      fixture.loopback_media_transports()->FirstStats();
  webrtc::MediaTransportPair::Stats second_stats =
      fixture.loopback_media_transports()->SecondStats();

  EXPECT_GT(first_stats.received_audio_frames, 0);
  EXPECT_GE(second_stats.sent_audio_frames, first_stats.received_audio_frames);

  EXPECT_GT(second_stats.received_audio_frames, 0);
  EXPECT_GE(first_stats.sent_audio_frames, second_stats.received_audio_frames);
}

TEST_P(PeerConnectionIntegrationTest, MediaTransportBidirectionalVideo) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.use_media_transport = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  fixture.caller()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();
  // Start offer/answer exchange and wait for it to complete.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that the media transport is ready.
  fixture.loopback_media_transports()->SetState(
      webrtc::MediaTransportState::kWritable);
  fixture.loopback_media_transports()->FlushAsyncInvokes();

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));

  webrtc::MediaTransportPair::Stats first_stats =
      fixture.loopback_media_transports()->FirstStats();
  webrtc::MediaTransportPair::Stats second_stats =
      fixture.loopback_media_transports()->SecondStats();

  EXPECT_GT(first_stats.received_video_frames, 0);
  EXPECT_GE(second_stats.sent_video_frames, first_stats.received_video_frames);

  EXPECT_GT(second_stats.received_video_frames, 0);
  EXPECT_GE(first_stats.sent_video_frames, second_stats.received_video_frames);
}

TEST_P(PeerConnectionIntegrationTest,
       MediaTransportDataChannelUsesRtpBidirectionalVideo) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.use_media_transport = false;
  rtc_config.use_media_transport_for_data_channels = true;
  rtc_config.enable_dtls_srtp = false;  // SDES is required for media transport.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(
      fixture.CreatePeerConnectionWrappersWithConfigAndMediaTransportFactory(
          rtc_config, rtc_config,
          fixture.loopback_media_transports()->first_factory(),
          fixture.loopback_media_transports()->second_factory()));
  fixture.ConnectFakeSignaling();

  fixture.caller()->AddVideoTrack();
  fixture.callee()->AddVideoTrack();
  // Start offer/answer exchange and wait for it to complete.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that the ICE connection and gathering states eventually reach
// "complete".
TEST_P(PeerConnectionIntegrationTest, IceStatesReachCompletion) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceGatheringComplete,
                 fixture.caller()->ice_gathering_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceGatheringComplete,
                 fixture.callee()->ice_gathering_state(), kMaxWaitForFramesMs);
  // After the best candidate pair is selected and all candidates are signaled,
  // the ICE connection state should reach "complete".
  // TODO(deadbeef): Currently, the ICE "controlled" agent (the
  // answerer/"callee" by default) only reaches "connected". When this is
  // fixed, this test should be updated.
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kDefaultTimeout);
}

constexpr int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                                cricket::PORTALLOCATOR_DISABLE_RELAY |
                                cricket::PORTALLOCATOR_DISABLE_TCP;

// Use a mock resolver to resolve the hostname back to the original IP on both
// sides and check that the ICE connection connects.
TEST_P(PeerConnectionIntegrationTest,
       IceStatesReachCompletionWithRemoteHostname) {
  auto caller_resolver_factory =
      absl::make_unique<NiceMock<webrtc::MockAsyncResolverFactory>>();
  auto callee_resolver_factory =
      absl::make_unique<NiceMock<webrtc::MockAsyncResolverFactory>>();
  NiceMock<rtc::MockAsyncResolver> callee_async_resolver;
  NiceMock<rtc::MockAsyncResolver> caller_async_resolver;

  // This also verifies that the injected AsyncResolverFactory is used by
  // P2PTransportChannel.
  EXPECT_CALL(*caller_resolver_factory, Create())
      .WillOnce(Return(&caller_async_resolver));
  webrtc::PeerConnectionDependencies caller_deps(nullptr);
  caller_deps.async_resolver_factory = std::move(caller_resolver_factory);

  EXPECT_CALL(*callee_resolver_factory, Create())
      .WillOnce(Return(&callee_async_resolver));
  webrtc::PeerConnectionDependencies callee_deps(nullptr);
  callee_deps.async_resolver_factory = std::move(callee_resolver_factory);

  PeerConnectionInterface::RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;

  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfigAndDeps(
      config, std::move(caller_deps), config, std::move(callee_deps)));

  fixture.caller()->SetRemoteAsyncResolver(&callee_async_resolver);
  fixture.callee()->SetRemoteAsyncResolver(&caller_async_resolver);

  // Enable hostname candidates with mDNS names.
  fixture.caller()->SetMdnsResponder(
      absl::make_unique<webrtc::FakeMdnsResponder>(fixture.network_thread()));
  fixture.callee()->SetMdnsResponder(
      absl::make_unique<webrtc::FakeMdnsResponder>(fixture.network_thread()));

  fixture.SetPortAllocatorFlags(kOnlyLocalPorts, kOnlyLocalPorts);

  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kDefaultTimeout);

  EXPECT_EQ(1, webrtc::metrics::NumEvents(
                   "WebRTC.PeerConnection.CandidatePairType_UDP",
                   webrtc::kIceCandidatePairHostNameHostName));
}

class PeerConnectionIntegrationIceStatesTestFixture
    : public PeerConnectionIntegrationTestFixture {
 public:
  PeerConnectionIntegrationIceStatesTestFixture(SdpSemantics sdp_semantics,
                                                uint32_t port_allocator_flags)
      : PeerConnectionIntegrationTestFixture(sdp_semantics) {
    port_allocator_flags_ = port_allocator_flags;
  }

  void StartStunServer(const SocketAddress& server_address) {
    stun_server_.reset(
        cricket::TestStunServer::Create(network_thread(), server_address));
  }

  bool TestIPv6() {
    return (port_allocator_flags_ & cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  void SetPortAllocatorFlags() {
    PeerConnectionIntegrationTestFixture::SetPortAllocatorFlags(
        port_allocator_flags_, port_allocator_flags_);
  }

  std::vector<SocketAddress> CallerAddresses() {
    std::vector<SocketAddress> addresses;
    addresses.push_back(SocketAddress("1.1.1.1", 0));
    if (TestIPv6()) {
      addresses.push_back(SocketAddress("1111:0:a:b:c:d:e:f", 0));
    }
    return addresses;
  }

  std::vector<SocketAddress> CalleeAddresses() {
    std::vector<SocketAddress> addresses;
    addresses.push_back(SocketAddress("2.2.2.2", 0));
    if (TestIPv6()) {
      addresses.push_back(SocketAddress("2222:0:a:b:c:d:e:f", 0));
    }
    return addresses;
  }

  void SetUpNetworkInterfaces() {
    // Remove the default interfaces added by the test infrastructure.
    caller()->network_manager()->RemoveInterface(kDefaultLocalAddress);
    callee()->network_manager()->RemoveInterface(kDefaultLocalAddress);

    // Add network addresses for test.
    for (const auto& caller_address : CallerAddresses()) {
      caller()->network_manager()->AddInterface(caller_address);
    }
    for (const auto& callee_address : CalleeAddresses()) {
      callee()->network_manager()->AddInterface(callee_address);
    }
  }

 private:
  uint32_t port_allocator_flags_;
  std::unique_ptr<cricket::TestStunServer> stun_server_;
};
// Test that firewalling the ICE connection causes the clients to
// identify the disconnected state and then removing the firewall causes
// them to reconnect.
class PeerConnectionIntegrationIceStatesTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, std::tuple<std::string, uint32_t>>> {
 protected:
  SdpSemantics GetSdpSemantics() const { return std::get<0>(GetParam()); }

  uint32_t GetPortAllocatorFlags() const {
    return std::get<1>(std::get<1>(GetParam()));
  }
};

// Tests that the PeerConnection goes through all the ICE gathering/connection
// states over the duration of the call. This includes Disconnected and Failed
// states, induced by putting a fixture.firewall between the peers and waiting
// for them to time out.
TEST_P(PeerConnectionIntegrationIceStatesTest, VerifyIceStates) {
  rtc::ScopedFakeClock fake_clock;
  // Some things use a time of "0" as a special value, so we need to start out
  // the fake clock at a nonzero time.
  fake_clock.AdvanceTime(TimeDelta::seconds(1));
  PeerConnectionIntegrationIceStatesTestFixture fixture(
      GetSdpSemantics(), GetPortAllocatorFlags());

  const SocketAddress kStunServerAddress =
      SocketAddress("99.99.99.1", cricket::STUN_SERVER_PORT);
  fixture.StartStunServer(kStunServerAddress);

  PeerConnectionInterface::RTCConfiguration config;
  PeerConnectionInterface::IceServer ice_stun_server;
  ice_stun_server.urls.push_back(
      "stun:" + kStunServerAddress.HostAsURIString() + ":" +
      kStunServerAddress.PortAsString());
  config.servers.push_back(ice_stun_server);

  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(config, config));
  fixture.ConnectFakeSignaling();
  fixture.SetPortAllocatorFlags();
  fixture.SetUpNetworkInterfaces();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();

  // Initial state before anything happens.
  ASSERT_EQ(PeerConnectionInterface::kIceGatheringNew,
            fixture.caller()->ice_gathering_state());
  ASSERT_EQ(PeerConnectionInterface::kIceConnectionNew,
            fixture.caller()->ice_connection_state());
  ASSERT_EQ(PeerConnectionInterface::kIceConnectionNew,
            fixture.caller()->standardized_ice_connection_state());

  // Start the call by creating the offer, setting it as the local description,
  // then sending it to the peer who will respond with an answer. This happens
  // asynchronously so that we can watch the states as it runs in the
  // background.
  fixture.caller()->CreateAndSetAndSignalOffer();

  ASSERT_EQ(PeerConnectionInterface::kIceConnectionCompleted,
            fixture.caller()->ice_connection_state());
  ASSERT_EQ(PeerConnectionInterface::kIceConnectionCompleted,
            fixture.caller()->standardized_ice_connection_state());

  // Verify that the observer was notified of the intermediate transitions.
  EXPECT_THAT(fixture.caller()->ice_connection_state_history(),
              ElementsAre(PeerConnectionInterface::kIceConnectionChecking,
                          PeerConnectionInterface::kIceConnectionConnected,
                          PeerConnectionInterface::kIceConnectionCompleted));
  EXPECT_THAT(fixture.caller()->standardized_ice_connection_state_history(),
              ElementsAre(PeerConnectionInterface::kIceConnectionChecking,
                          PeerConnectionInterface::kIceConnectionConnected,
                          PeerConnectionInterface::kIceConnectionCompleted));
  EXPECT_THAT(
      fixture.caller()->peer_connection_state_history(),
      ElementsAre(PeerConnectionInterface::PeerConnectionState::kConnecting,
                  PeerConnectionInterface::PeerConnectionState::kConnected));
  EXPECT_THAT(fixture.caller()->ice_gathering_state_history(),
              ElementsAre(PeerConnectionInterface::kIceGatheringGathering,
                          PeerConnectionInterface::kIceGatheringComplete));

  // Block connections to/from the caller and wait for ICE to become
  // disconnected.
  for (const auto& caller_address : fixture.CallerAddresses()) {
    fixture.firewall()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY,
                                caller_address);
  }
  RTC_LOG(LS_INFO) << "Firewall rules applied";
  ASSERT_EQ_SIMULATED_WAIT(PeerConnectionInterface::kIceConnectionDisconnected,
                           fixture.caller()->ice_connection_state(),
                           kDefaultTimeout, fake_clock);
  ASSERT_EQ_SIMULATED_WAIT(
      PeerConnectionInterface::kIceConnectionDisconnected,
      fixture.caller()->standardized_ice_connection_state(), kDefaultTimeout,
      fake_clock);

  // Let ICE re-establish by removing the fixture.firewall rules.
  fixture.firewall()->ClearRules();
  RTC_LOG(LS_INFO) << "Firewall rules cleared";
  ASSERT_EQ_SIMULATED_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                           fixture.caller()->ice_connection_state(),
                           kDefaultTimeout, fake_clock);
  ASSERT_EQ_SIMULATED_WAIT(
      PeerConnectionInterface::kIceConnectionCompleted,
      fixture.caller()->standardized_ice_connection_state(), kDefaultTimeout,
      fake_clock);

  // According to RFC7675, if there is no response within 30 seconds then the
  // peer should consider the other side to have rejected the connection. This
  // is signaled by the state transitioning to "failed".
  constexpr int kConsentTimeout = 30000;
  for (const auto& caller_address : fixture.CallerAddresses()) {
    fixture.firewall()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY,
                                caller_address);
  }
  RTC_LOG(LS_INFO) << "Firewall rules applied again";
  ASSERT_EQ_SIMULATED_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                           fixture.caller()->ice_connection_state(),
                           kConsentTimeout, fake_clock);
  ASSERT_EQ_SIMULATED_WAIT(
      PeerConnectionInterface::kIceConnectionFailed,
      fixture.caller()->standardized_ice_connection_state(), kConsentTimeout,
      fake_clock);

  // We need to manually close the peerconnections before the fake clock goes
  // out of scope, or we trigger a DCHECK in rtp_sender.cc when we briefly
  // return to using non-faked time.
  delete fixture.SetCallerPcWrapperAndReturnCurrent(nullptr);
  delete fixture.SetCalleePcWrapperAndReturnCurrent(nullptr);
}

// Tests that if the connection doesn't get set up properly we eventually reach
// the "failed" iceConnectionState.
TEST_P(PeerConnectionIntegrationIceStatesTest, IceStateSetupFailure) {
  rtc::ScopedFakeClock fake_clock;
  // Some things use a time of "0" as a special value, so we need to start out
  // the fake clock at a nonzero time.
  fake_clock.AdvanceTime(TimeDelta::seconds(1));

  PeerConnectionIntegrationIceStatesTestFixture fixture(
      GetSdpSemantics(), GetPortAllocatorFlags());
  // Block connections to/from the caller and wait for ICE to become
  // disconnected.
  for (const auto& caller_address : fixture.CallerAddresses()) {
    fixture.firewall()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY,
                                caller_address);
  }

  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.SetPortAllocatorFlags();
  fixture.SetUpNetworkInterfaces();
  fixture.caller()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();

  // According to RFC7675, if there is no response within 30 seconds then the
  // peer should consider the other side to have rejected the connection. This
  // is signaled by the state transitioning to "failed".
  constexpr int kConsentTimeout = 30000;
  ASSERT_EQ_SIMULATED_WAIT(
      PeerConnectionInterface::kIceConnectionFailed,
      fixture.caller()->standardized_ice_connection_state(), kConsentTimeout,
      fake_clock);

  // We need to manually close the peerconnections before the fake clock goes
  // out of scope, or we trigger a DCHECK in rtp_sender.cc when we briefly
  // return to using non-faked time.
  delete fixture.SetCallerPcWrapperAndReturnCurrent(nullptr);
  delete fixture.SetCalleePcWrapperAndReturnCurrent(nullptr);
}

// Tests that the best connection is set to the appropriate IPv4/IPv6 connection
// and that the statistics in the metric observers are updated correctly.
TEST_P(PeerConnectionIntegrationIceStatesTest, VerifyBestConnection) {
  PeerConnectionIntegrationIceStatesTestFixture fixture(
      GetSdpSemantics(), GetPortAllocatorFlags());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.SetPortAllocatorFlags();
  fixture.SetUpNetworkInterfaces();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();

  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // TODO(bugs.webrtc.org/9456): Fix it.
  const int num_best_ipv4 = webrtc::metrics::NumEvents(
      "WebRTC.PeerConnection.IPMetrics", webrtc::kBestConnections_IPv4);
  const int num_best_ipv6 = webrtc::metrics::NumEvents(
      "WebRTC.PeerConnection.IPMetrics", webrtc::kBestConnections_IPv6);
  if (fixture.TestIPv6()) {
    // When IPv6 is enabled, we should prefer an IPv6 connection over an IPv4
    // connection.
    EXPECT_EQ(0, num_best_ipv4);
    EXPECT_EQ(1, num_best_ipv6);
  } else {
    EXPECT_EQ(1, num_best_ipv4);
    EXPECT_EQ(0, num_best_ipv6);
  }

  EXPECT_EQ(0, webrtc::metrics::NumEvents(
                   "WebRTC.PeerConnection.CandidatePairType_UDP",
                   webrtc::kIceCandidatePairHostHost));
  EXPECT_EQ(1, webrtc::metrics::NumEvents(
                   "WebRTC.PeerConnection.CandidatePairType_UDP",
                   webrtc::kIceCandidatePairHostPublicHostPublic));
}

constexpr uint32_t kFlagsIPv4NoStun = cricket::PORTALLOCATOR_DISABLE_TCP |
                                      cricket::PORTALLOCATOR_DISABLE_STUN |
                                      cricket::PORTALLOCATOR_DISABLE_RELAY;
constexpr uint32_t kFlagsIPv6NoStun =
    cricket::PORTALLOCATOR_DISABLE_TCP | cricket::PORTALLOCATOR_DISABLE_STUN |
    cricket::PORTALLOCATOR_ENABLE_IPV6 | cricket::PORTALLOCATOR_DISABLE_RELAY;
constexpr uint32_t kFlagsIPv4Stun =
    cricket::PORTALLOCATOR_DISABLE_TCP | cricket::PORTALLOCATOR_DISABLE_RELAY;

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionIntegrationTest,
    PeerConnectionIntegrationIceStatesTest,
    Combine(Values(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
            Values(std::make_pair("IPv4 no STUN", kFlagsIPv4NoStun),
                   std::make_pair("IPv6 no STUN", kFlagsIPv6NoStun),
                   std::make_pair("IPv4 with STUN", kFlagsIPv4Stun))));

// This test sets up a call between two parties with audio and video.
// During the call, the caller restarts ICE and the test verifies that
// new ICE candidates are generated and audio and video still can flow, and the
// ICE state reaches completed again.
TEST_P(PeerConnectionIntegrationTest, MediaContinuesFlowingAfterIceRestart) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer and wait for ICE to complete.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kMaxWaitForFramesMs);

  // To verify that the ICE restart actually occurs, get
  // ufrag/password/candidates before and after restart.
  // Create an SDP string of the first audio candidate for both clients.
  const webrtc::IceCandidateCollection* audio_candidates_caller =
      fixture.caller()->pc()->local_description()->candidates(0);
  const webrtc::IceCandidateCollection* audio_candidates_callee =
      fixture.callee()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_caller->count(), 0u);
  ASSERT_GT(audio_candidates_callee->count(), 0u);
  std::string caller_candidate_pre_restart;
  ASSERT_TRUE(
      audio_candidates_caller->at(0)->ToString(&caller_candidate_pre_restart));
  std::string callee_candidate_pre_restart;
  ASSERT_TRUE(
      audio_candidates_callee->at(0)->ToString(&callee_candidate_pre_restart));
  const cricket::SessionDescription* desc =
      fixture.caller()->pc()->local_description()->description();
  std::string caller_ufrag_pre_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  desc = fixture.callee()->pc()->local_description()->description();
  std::string callee_ufrag_pre_restart =
      desc->transport_infos()[0].description.ice_ufrag;

  // Have the caller initiate an ICE restart.
  fixture.caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kMaxWaitForFramesMs);

  // Grab the ufrags/candidates again.
  audio_candidates_caller =
      fixture.caller()->pc()->local_description()->candidates(0);
  audio_candidates_callee =
      fixture.callee()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_caller->count(), 0u);
  ASSERT_GT(audio_candidates_callee->count(), 0u);
  std::string caller_candidate_post_restart;
  ASSERT_TRUE(
      audio_candidates_caller->at(0)->ToString(&caller_candidate_post_restart));
  std::string callee_candidate_post_restart;
  ASSERT_TRUE(
      audio_candidates_callee->at(0)->ToString(&callee_candidate_post_restart));
  desc = fixture.caller()->pc()->local_description()->description();
  std::string caller_ufrag_post_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  desc = fixture.callee()->pc()->local_description()->description();
  std::string callee_ufrag_post_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  // Sanity check that an ICE restart was actually negotiated in SDP.
  ASSERT_NE(caller_candidate_pre_restart, caller_candidate_post_restart);
  ASSERT_NE(callee_candidate_pre_restart, callee_candidate_post_restart);
  ASSERT_NE(caller_ufrag_pre_restart, caller_ufrag_post_restart);
  ASSERT_NE(callee_ufrag_pre_restart, callee_ufrag_post_restart);

  // Ensure that additional frames are received after the ICE restart.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Verify that audio/video can be received end-to-end when ICE renomination is
// enabled.
TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithIceRenomination) {
  PeerConnectionInterface::RTCConfiguration config;
  config.enable_ice_renomination = true;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(config, config));
  fixture.ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Sanity check that ICE renomination was actually negotiated.
  const cricket::SessionDescription* desc =
      fixture.caller()->pc()->local_description()->description();
  for (const cricket::TransportInfo& info : desc->transport_infos()) {
    ASSERT_THAT(info.description.transport_options, Contains("renomination"));
  }
  desc = fixture.callee()->pc()->local_description()->description();
  for (const cricket::TransportInfo& info : desc->transport_infos()) {
    ASSERT_THAT(info.description.transport_options, Contains("renomination"));
  }
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// With a max bundle policy and RTCP muxing, adding a new media description to
// the connection should not affect ICE at all because the new media will use
// the existing connection.
TEST_P(PeerConnectionIntegrationTest,
       AddMediaToConnectedBundleDoesNotRestartIce) {
  PeerConnectionInterface::RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(
      config, PeerConnectionInterface::RTCConfiguration()));
  fixture.ConnectFakeSignaling();

  fixture.caller()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kDefaultTimeout);

  fixture.caller()->clear_ice_connection_state_history();

  fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  EXPECT_EQ(0u, fixture.caller()->ice_connection_state_history().size());
}

// This test sets up a call between two parties with audio and video. It then
// renegotiates setting the video m-line to "port 0", then later renegotiates
// again, enabling video.
TEST_P(PeerConnectionIntegrationTest,
       VideoFlowsAfterMediaSectionIsRejectedAndRecycled) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Do initial negotiation, only sending media from the caller. Will result in
  // video and audio recvonly "m=" sections.
  fixture.caller()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Negotiate again, disabling the video "m=" section (the callee will set the
  // port to 0 due to offer_to_receive_video = 0).
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 0;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    fixture.callee()->SetRemoteOfferHandler([&] {
      fixture.callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
          ->Stop();
    });
  }
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Sanity check that video "m=" section was actually rejected.
  const ContentInfo* answer_video_content = cricket::GetFirstVideoContent(
      fixture.callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, answer_video_content);
  ASSERT_TRUE(answer_video_content->rejected);

  // Enable video and do negotiation again, making sure video is received
  // end-to-end, also adding media stream to callee.
  if (fixture.sdp_semantics_ == SdpSemantics::kPlanB) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 1;
    fixture.callee()->SetOfferAnswerOptions(options);
  } else {
    // The caller's transceiver is stopped, so we need to add another track.
    auto caller_transceiver =
        fixture.caller()->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO);
    EXPECT_TRUE(caller_transceiver->stopped());
    fixture.caller()->AddVideoTrack();
  }
  fixture.callee()->AddVideoTrack();
  fixture.callee()->SetRemoteOfferHandler(nullptr);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Verify the caller receives frames from the newly added stream, and the
  // callee receives additional frames from the re-enabled video m= section.
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This tests that if we negotiate after calling CreateSender but before we
// have a track, then set a track later, frames from the newly-set track are
// received end-to-end.
TEST_F(PeerConnectionIntegrationTestPlanB,
       MediaFlowsAfterEarlyWarmupWithCreateSender) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  auto caller_audio_sender =
      fixture.caller()->pc()->CreateSender("audio", "caller_stream");
  auto caller_video_sender =
      fixture.caller()->pc()->CreateSender("video", "caller_stream");
  auto callee_audio_sender =
      fixture.callee()->pc()->CreateSender("audio", "callee_stream");
  auto callee_video_sender =
      fixture.callee()->pc()->CreateSender("video", "callee_stream");
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kMaxWaitForActivationMs);
  // Wait for ICE to complete, without any tracks being set.
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kMaxWaitForFramesMs);
  // Now set the tracks, and expect frames to immediately start flowing.
  EXPECT_TRUE(
      caller_audio_sender->SetTrack(fixture.caller()->CreateLocalAudioTrack()));
  EXPECT_TRUE(
      caller_video_sender->SetTrack(fixture.caller()->CreateLocalVideoTrack()));
  EXPECT_TRUE(
      callee_audio_sender->SetTrack(fixture.callee()->CreateLocalAudioTrack()));
  EXPECT_TRUE(
      callee_video_sender->SetTrack(fixture.callee()->CreateLocalVideoTrack()));
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This tests that if we negotiate after calling AddTransceiver but before we
// have a track, then set a track later, frames from the newly-set tracks are
// received end-to-end.
TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       MediaFlowsAfterEarlyWarmupWithAddTransceiver) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  auto audio_result =
      fixture.caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  ASSERT_EQ(RTCErrorType::NONE, audio_result.error().type());
  auto caller_audio_sender = audio_result.MoveValue()->sender();
  auto video_result =
      fixture.caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  ASSERT_EQ(RTCErrorType::NONE, video_result.error().type());
  auto caller_video_sender = video_result.MoveValue()->sender();
  fixture.callee()->SetRemoteOfferHandler([&] {
    ASSERT_EQ(2u, fixture.callee()->pc()->GetTransceivers().size());
    fixture.callee()->pc()->GetTransceivers()[0]->SetDirection(
        RtpTransceiverDirection::kSendRecv);
    fixture.callee()->pc()->GetTransceivers()[1]->SetDirection(
        RtpTransceiverDirection::kSendRecv);
  });
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kMaxWaitForActivationMs);
  // Wait for ICE to complete, without any tracks being set.
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 fixture.caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kMaxWaitForFramesMs);
  // Now set the tracks, and expect frames to immediately start flowing.
  auto callee_audio_sender = fixture.callee()->pc()->GetSenders()[0];
  auto callee_video_sender = fixture.callee()->pc()->GetSenders()[1];
  ASSERT_TRUE(
      caller_audio_sender->SetTrack(fixture.caller()->CreateLocalAudioTrack()));
  ASSERT_TRUE(
      caller_video_sender->SetTrack(fixture.caller()->CreateLocalVideoTrack()));
  ASSERT_TRUE(
      callee_audio_sender->SetTrack(fixture.callee()->CreateLocalAudioTrack()));
  ASSERT_TRUE(
      callee_video_sender->SetTrack(fixture.callee()->CreateLocalVideoTrack()));
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// This test verifies that a remote video track can be added via AddStream,
// and sent end-to-end. For this particular test, it's simply echoed back
// from the caller to the callee, rather than being forwarded to a third
// PeerConnection.
TEST_F(PeerConnectionIntegrationTestPlanB, CanSendRemoteVideoTrack) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  // Just send a video track from the caller.
  fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kMaxWaitForActivationMs);
  ASSERT_EQ(1U, fixture.callee()->remote_streams()->count());

  // Echo the stream back, and do a new offer/anwer (initiated by callee this
  // time).
  fixture.callee()->pc()->AddStream(fixture.callee()->remote_streams()->at(0));
  fixture.callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kMaxWaitForActivationMs);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that we achieve the expected end-to-end connection time, using a
// fake clock and simulated latency on the media and signaling paths.
// We use a TURN<->TURN connection because this is usually the quickest to
// set up initially, especially when we're confident the connection will work
// and can start sending media before we get a STUN response.
//
// With various optimizations enabled, here are the network delays we expect to
// be on the critical path:
// 1. 2 signaling trips: Signaling offer and offerer's TURN candidate, then
//                       signaling answer (with DTLS fingerprint).
// 2. 9 media hops: Rest of the DTLS handshake. 3 hops in each direction when
//                  using TURN<->TURN pair, and DTLS exchange is 4 packets,
//                  the first of which should have arrived before the answer.
TEST_P(PeerConnectionIntegrationTest, EndToEndConnectionTimeWithTurnTurnPair) {
  rtc::ScopedFakeClock fake_clock;
  // Some things use a time of "0" as a special value, so we need to start out
  // the fake clock at a nonzero time.
  // TODO(deadbeef): Fix this.
  fake_clock.AdvanceTime(webrtc::TimeDelta::seconds(1));

  // The fixture is created after clock to ensure that PeerConnections are
  // destroyed before ScopedFakeClock. If this is not done a DCHECK can be hit
  // in ports.cc, because a large negative number is calculated for the rtt due
  // to the global clock changing.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());

  static constexpr int media_hop_delay_ms = 50;
  static constexpr int signaling_trip_delay_ms = 500;
  // For explanation of these values, see comment above.
  static constexpr int required_media_hops = 9;
  static constexpr int required_signaling_trips = 2;
  // For internal delays (such as posting an event asychronously).
  static constexpr int allowed_internal_delay_ms = 20;
  static constexpr int total_connection_time_ms =
      media_hop_delay_ms * required_media_hops +
      signaling_trip_delay_ms * required_signaling_trips +
      allowed_internal_delay_ms;

  static const rtc::SocketAddress turn_server_1_internal_address{"88.88.88.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_1_external_address{"88.88.88.1",
                                                                 0};
  static const rtc::SocketAddress turn_server_2_internal_address{"99.99.99.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_2_external_address{"99.99.99.1",
                                                                 0};
  cricket::TestTurnServer* turn_server_1 = fixture.CreateTurnServer(
      turn_server_1_internal_address, turn_server_1_external_address);

  cricket::TestTurnServer* turn_server_2 = fixture.CreateTurnServer(
      turn_server_2_internal_address, turn_server_2_external_address);
  // Bypass permission check on received packets so media can be sent before
  // the candidate is signaled.
  fixture.network_thread()->Invoke<void>(RTC_FROM_HERE, [turn_server_1] {
    turn_server_1->set_enable_permission_checks(false);
  });
  fixture.network_thread()->Invoke<void>(RTC_FROM_HERE, [turn_server_2] {
    turn_server_2->set_enable_permission_checks(false);
  });

  PeerConnectionInterface::RTCConfiguration client_1_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_1;
  ice_server_1.urls.push_back("turn:88.88.88.0:3478");
  ice_server_1.username = "test";
  ice_server_1.password = "test";
  client_1_config.servers.push_back(ice_server_1);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;
  client_1_config.presume_writable_when_fully_relayed = true;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_2;
  ice_server_2.urls.push_back("turn:99.99.99.0:3478");
  ice_server_2.username = "test";
  ice_server_2.password = "test";
  client_2_config.servers.push_back(ice_server_2);
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;
  client_2_config.presume_writable_when_fully_relayed = true;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(client_1_config,
                                                             client_2_config));
  // Set up the simulated delays.
  fixture.SetSignalingDelayMs(signaling_trip_delay_ms);
  fixture.ConnectFakeSignaling();
  fixture.virtual_socket_server()->set_delay_mean(media_hop_delay_ms);
  fixture.virtual_socket_server()->UpdateDelayDistribution();

  // Set "offer to receive audio/video" without adding any tracks, so we just
  // set up ICE/DTLS with no media.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  EXPECT_TRUE_SIMULATED_WAIT(fixture.DtlsConnected(), total_connection_time_ms,
                             fake_clock);
}

// Verify that a TurnCustomizer passed in through RTCConfiguration
// is actually used by the underlying TURN candidate pair.
// Note that turnport_unittest.cc contains more detailed, lower-level tests.
TEST_P(PeerConnectionIntegrationTest, TurnCustomizerUsedForTurnConnections) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());

  static const rtc::SocketAddress turn_server_1_internal_address{"88.88.88.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_1_external_address{"88.88.88.1",
                                                                 0};
  static const rtc::SocketAddress turn_server_2_internal_address{"99.99.99.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_2_external_address{"99.99.99.1",
                                                                 0};
  fixture.CreateTurnServer(turn_server_1_internal_address,
                           turn_server_1_external_address);
  fixture.CreateTurnServer(turn_server_2_internal_address,
                           turn_server_2_external_address);

  PeerConnectionInterface::RTCConfiguration client_1_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_1;
  ice_server_1.urls.push_back("turn:88.88.88.0:3478");
  ice_server_1.username = "test";
  ice_server_1.password = "test";
  client_1_config.servers.push_back(ice_server_1);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;
  auto* customizer1 = fixture.CreateTurnCustomizer();
  client_1_config.turn_customizer = customizer1;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_2;
  ice_server_2.urls.push_back("turn:99.99.99.0:3478");
  ice_server_2.username = "test";
  ice_server_2.password = "test";
  client_2_config.servers.push_back(ice_server_2);
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;
  auto* customizer2 = fixture.CreateTurnCustomizer();
  client_2_config.turn_customizer = customizer2;

  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(client_1_config,
                                                             client_2_config));
  fixture.ConnectFakeSignaling();

  // Set "offer to receive audio/video" without adding any tracks, so we just
  // set up ICE/DTLS with no media.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);

  fixture.ExpectTurnCustomizerCountersIncremented(customizer1);
  fixture.ExpectTurnCustomizerCountersIncremented(customizer2);
}

// Verifies that you can use TCP instead of UDP to connect to a TURN server and
// send media between the caller and the callee.
TEST_P(PeerConnectionIntegrationTest, TCPUsedForTurnConnections) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());

  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  // Enable TCP for the fake turn server.
  fixture.CreateTurnServer(turn_server_internal_address,
                           turn_server_external_address, cricket::PROTO_TCP);

  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:88.88.88.0:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.servers.push_back(ice_server);
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;

  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(client_1_config,
                                                             client_2_config));

  // Do normal offer/answer and wait for ICE to complete.
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kMaxWaitForFramesMs);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  EXPECT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Verify that a SSLCertificateVerifier passed in through
// PeerConnectionDependencies is actually used by the underlying SSL
// implementation to determine whether a certificate presented by the TURN
// server is accepted by the client. Note that openssladapter_unittest.cc
// contains more detailed, lower-level tests.
TEST_P(PeerConnectionIntegrationTest,
       SSLCertificateVerifierUsedForTurnConnections) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  // Enable TCP-TLS for the fake turn server. We need to pass in 88.88.88.0 so
  // that host name verification passes on the fake certificate.
  fixture.CreateTurnServer(turn_server_internal_address,
                           turn_server_external_address, cricket::PROTO_TLS,
                           "88.88.88.0");

  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turns:88.88.88.0:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.servers.push_back(ice_server);
  // Setting the type to kRelay forces the connection to go through a TURN
  // server.
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;

  // Get a copy to the pointer so we can verify calls later.
  rtc::TestCertificateVerifier* client_1_cert_verifier =
      new rtc::TestCertificateVerifier();
  client_1_cert_verifier->verify_certificate_ = true;
  rtc::TestCertificateVerifier* client_2_cert_verifier =
      new rtc::TestCertificateVerifier();
  client_2_cert_verifier->verify_certificate_ = true;

  // Create the dependencies with the test certificate verifier.
  webrtc::PeerConnectionDependencies client_1_deps(nullptr);
  client_1_deps.tls_cert_verifier =
      std::unique_ptr<rtc::TestCertificateVerifier>(client_1_cert_verifier);
  webrtc::PeerConnectionDependencies client_2_deps(nullptr);
  client_2_deps.tls_cert_verifier =
      std::unique_ptr<rtc::TestCertificateVerifier>(client_2_cert_verifier);
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfigAndDeps(
      client_1_config, std::move(client_1_deps), client_2_config,
      std::move(client_2_deps)));
  fixture.ConnectFakeSignaling();

  // Set "offer to receive audio/video" without adding any tracks, so we just
  // set up ICE/DTLS with no media.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);

  EXPECT_GT(client_1_cert_verifier->call_count_, 0u);
  EXPECT_GT(client_2_cert_verifier->call_count_, 0u);
}

TEST_P(PeerConnectionIntegrationTest,
       SSLCertificateVerifierFailureUsedForTurnConnectionsFailsConnection) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  // Enable TCP-TLS for the fake turn server. We need to pass in 88.88.88.0 so
  // that host name verification passes on the fake certificate.
  fixture.CreateTurnServer(turn_server_internal_address,
                           turn_server_external_address, cricket::PROTO_TLS,
                           "88.88.88.0");

  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turns:88.88.88.0:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.servers.push_back(ice_server);
  // Setting the type to kRelay forces the connection to go through a TURN
  // server.
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;

  // Get a copy to the pointer so we can verify calls later.
  rtc::TestCertificateVerifier* client_1_cert_verifier =
      new rtc::TestCertificateVerifier();
  client_1_cert_verifier->verify_certificate_ = false;
  rtc::TestCertificateVerifier* client_2_cert_verifier =
      new rtc::TestCertificateVerifier();
  client_2_cert_verifier->verify_certificate_ = false;

  // Create the dependencies with the test certificate verifier.
  webrtc::PeerConnectionDependencies client_1_deps(nullptr);
  client_1_deps.tls_cert_verifier =
      std::unique_ptr<rtc::TestCertificateVerifier>(client_1_cert_verifier);
  webrtc::PeerConnectionDependencies client_2_deps(nullptr);
  client_2_deps.tls_cert_verifier =
      std::unique_ptr<rtc::TestCertificateVerifier>(client_2_cert_verifier);
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfigAndDeps(
      client_1_config, std::move(client_1_deps), client_2_config,
      std::move(client_2_deps)));
  fixture.ConnectFakeSignaling();

  // Set "offer to receive audio/video" without adding any tracks, so we just
  // set up ICE/DTLS with no media.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  bool wait_res = true;
  // TODO(bugs.webrtc.org/9219): When IceConnectionState is implemented
  // properly, should be able to just wait for a state of "failed" instead of
  // waiting a fixed 10 seconds.
  WAIT_(fixture.DtlsConnected(), kDefaultTimeout, wait_res);
  ASSERT_FALSE(wait_res);

  EXPECT_GT(client_1_cert_verifier->call_count_, 0u);
  EXPECT_GT(client_2_cert_verifier->call_count_, 0u);
}

// Test that audio and video flow end-to-end when codec names don't use the
// expected casing, given that they're supposed to be case insensitive. To test
// this, all but one codec is removed from each media description, and its
// casing is changed.
//
// In the past, this has regressed and caused crashes/black video, due to the
// fact that code at some layers was doing case-insensitive comparisons and
// code at other layers was not.
TEST_P(PeerConnectionIntegrationTest, CodecNamesAreCaseInsensitive) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();

  // Remove all but one audio/video codec (opus and VP8), and change the
  // casing of the caller's generated offer.
  fixture.caller()->SetGeneratedSdpMunger([](cricket::SessionDescription*
                                                 description) {
    cricket::AudioContentDescription* audio =
        GetFirstAudioContentDescription(description);
    ASSERT_NE(nullptr, audio);
    auto audio_codecs = audio->codecs();
    audio_codecs.erase(std::remove_if(audio_codecs.begin(), audio_codecs.end(),
                                      [](const cricket::AudioCodec& codec) {
                                        return codec.name != "opus";
                                      }),
                       audio_codecs.end());
    ASSERT_EQ(1u, audio_codecs.size());
    audio_codecs[0].name = "OpUs";
    audio->set_codecs(audio_codecs);

    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(description);
    ASSERT_NE(nullptr, video);
    auto video_codecs = video->codecs();
    video_codecs.erase(std::remove_if(video_codecs.begin(), video_codecs.end(),
                                      [](const cricket::VideoCodec& codec) {
                                        return codec.name != "VP8";
                                      }),
                       video_codecs.end());
    ASSERT_EQ(1u, video_codecs.size());
    video_codecs[0].name = "vP8";
    video->set_codecs(video_codecs);
  });

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Verify frames are still received end-to-end.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, GetSourcesAudio) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for one audio frame to be received by the callee.
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  ASSERT_EQ(fixture.callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = fixture.callee()->pc()->GetReceivers()[0];
  ASSERT_EQ(receiver->media_type(), cricket::MEDIA_TYPE_AUDIO);
  auto sources = receiver->GetSources();
  ASSERT_GT(receiver->GetParameters().encodings.size(), 0u);
  EXPECT_EQ(receiver->GetParameters().encodings[0].ssrc,
            sources[0].source_id());
  EXPECT_EQ(webrtc::RtpSourceType::SSRC, sources[0].source_type());
}

TEST_P(PeerConnectionIntegrationTest, GetSourcesVideo) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddVideoTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for one video frame to be received by the callee.
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo(1);
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  ASSERT_EQ(fixture.callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = fixture.callee()->pc()->GetReceivers()[0];
  ASSERT_EQ(receiver->media_type(), cricket::MEDIA_TYPE_VIDEO);
  auto sources = receiver->GetSources();
  ASSERT_GT(receiver->GetParameters().encodings.size(), 0u);
  EXPECT_EQ(receiver->GetParameters().encodings[0].ssrc,
            sources[0].source_id());
  EXPECT_EQ(webrtc::RtpSourceType::SSRC, sources[0].source_type());
}

// Test that if a track is removed and added again with a different stream ID,
// the new stream ID is successfully communicated in SDP and media continues to
// flow end-to-end.
// TODO(webrtc.bugs.org/8734): This test does not work for Unified Plan because
// it will not reuse a transceiver that has already been sending. After creating
// a new transceiver it tries to create an offer with two senders of the same
// track ids and it fails.
TEST_F(PeerConnectionIntegrationTestPlanB, RemoveAndAddTrackWithNewStreamId) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Add track using stream 1, do offer/answer.
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      fixture.caller()->CreateLocalAudioTrack();
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender =
      fixture.caller()->AddTrack(track, {"stream_1"});
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio(1);
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
  // Remove the sender, and create a new one with the new stream.
  fixture.caller()->pc()->RemoveTrack(sender);
  sender = fixture.caller()->AddTrack(track, {"stream_2"});
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Wait for additional audio frames to be received by the callee.
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
}

TEST_P(PeerConnectionIntegrationTest, RtcEventLogOutputWriteCalled) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  auto output = absl::make_unique<testing::NiceMock<MockRtcEventLogOutput>>();
  ON_CALL(*output, IsActive()).WillByDefault(::testing::Return(true));
  ON_CALL(*output, Write(::testing::_)).WillByDefault(::testing::Return(true));
  EXPECT_CALL(*output, Write(::testing::_)).Times(::testing::AtLeast(1));
  EXPECT_TRUE(fixture.caller()->pc()->StartRtcEventLog(
      std::move(output), webrtc::RtcEventLog::kImmediateOutput));

  fixture.caller()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
}

// Test that if candidates are only signaled by applying full session
// descriptions (instead of using AddIceCandidate), the peers can connect to
// each other and exchange media.
TEST_P(PeerConnectionIntegrationTest, MediaFlowsWhenCandidatesSetOnlyInSdp) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  // Each side will signal the session descriptions but not candidates.
  fixture.ConnectFakeSignalingForSdpOnly();

  // Add audio video track and exchange the initial offer/answer with media
  // information only. This will start ICE gathering on each side.
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();

  // Wait for all candidates to be gathered on both the caller and callee.
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 fixture.caller()->ice_gathering_state(), kDefaultTimeout);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 fixture.callee()->ice_gathering_state(), kDefaultTimeout);

  // The candidates will now be included in the session description, so
  // signaling them will start the ICE connection.
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Ensure that media flows in both directions.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that SetAudioPlayout can be used to disable audio playout from the
// start, then later enable it. This may be useful, for example, if the caller
// needs to play a local ringtone until some event occurs, after which it
// switches to playing the received audio.
TEST_P(PeerConnectionIntegrationTest, DisableAndEnableAudioPlayout) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Set up audio-only call where audio playout is disabled on caller's side.
  fixture.caller()->pc()->SetAudioPlayout(false);
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Pump messages for a second.
  WAIT(false, 1000);
  // Since audio playout is disabled, the caller shouldn't have received
  // anything (at the playout level, at least).
  EXPECT_EQ(0, fixture.caller()->audio_frames_received());
  // As a sanity check, make sure the callee (for which playout isn't disabled)
  // did still see frames on its audio level.
  ASSERT_GT(fixture.callee()->audio_frames_received(), 0);

  // Enable playout again, and ensure audio starts flowing.
  fixture.caller()->pc()->SetAudioPlayout(true);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

double GetAudioEnergyStat(PeerConnectionWrapper* pc) {
  auto report = pc->NewGetStats();
  auto track_stats_list =
      report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  const webrtc::RTCMediaStreamTrackStats* remote_track_stats = nullptr;
  for (const auto* track_stats : track_stats_list) {
    if (track_stats->remote_source.is_defined() &&
        *track_stats->remote_source) {
      remote_track_stats = track_stats;
      break;
    }
  }

  if (!remote_track_stats->total_audio_energy.is_defined()) {
    return 0.0;
  }
  return *remote_track_stats->total_audio_energy;
}

// Test that if audio playout is disabled via the SetAudioPlayout() method, then
// incoming audio is still processed and statistics are generated.
TEST_P(PeerConnectionIntegrationTest,
       DisableAudioPlayoutStillGeneratesAudioStats) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Set up audio-only call where playout is disabled but audio-processing is
  // still active.
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->pc()->SetAudioPlayout(false);

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Wait for the callee to receive audio stats.
  EXPECT_TRUE_WAIT(GetAudioEnergyStat(fixture.caller()) > 0,
                   kMaxWaitForFramesMs);
}

// Test that SetAudioRecording can be used to disable audio recording from the
// start, then later enable it. This may be useful, for example, if the caller
// wants to ensure that no audio resources are active before a certain state
// is reached.
TEST_P(PeerConnectionIntegrationTest, DisableAndEnableAudioRecording) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();

  // Set up audio-only call where audio recording is disabled on caller's side.
  fixture.caller()->pc()->SetAudioRecording(false);
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddAudioTrack();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Pump messages for a second.
  WAIT(false, 1000);
  // Since caller has disabled audio recording, the callee shouldn't have
  // received anything.
  EXPECT_EQ(0, fixture.callee()->audio_frames_received());
  // As a sanity check, make sure the caller did still see frames on its
  // audio level since audio recording is enabled on the calle side.
  ASSERT_GT(fixture.caller()->audio_frames_received(), 0);

  // Enable audio recording again, and ensure audio starts flowing.
  fixture.caller()->pc()->SetAudioRecording(true);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that after closing PeerConnections, they stop sending any packets (ICE,
// DTLS, RTP...).
TEST_P(PeerConnectionIntegrationTest, ClosingConnectionStopsPacketFlow) {
  // Set up audio/video/data, wait for some frames to be received.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
#ifdef HAVE_SCTP
  fixture.caller()->CreateDataChannel();
#endif
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  // Close PeerConnections.
  fixture.ClosePeerConnections();
  // Pump messages for a second, and ensure no new packets end up sent.
  uint32_t sent_packets_a = fixture.virtual_socket_server()->sent_packets();
  WAIT(false, 1000);
  uint32_t sent_packets_b = fixture.virtual_socket_server()->sent_packets();
  EXPECT_EQ(sent_packets_a, sent_packets_b);
}

// Test that transport stats are generated by the RTCStatsCollector for a
// connection that only involves data channels. This is a regression test for
// crbug.com/826972.
#ifdef HAVE_SCTP
TEST_P(PeerConnectionIntegrationTest,
       TransportStatsReportedForDataChannelOnlyConnection) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(fixture.callee()->data_channel(), kDefaultTimeout);

  auto caller_report = fixture.caller()->NewGetStats();
  EXPECT_EQ(1u, caller_report->GetStatsOfType<RTCTransportStats>().size());
  auto callee_report = fixture.callee()->NewGetStats();
  EXPECT_EQ(1u, callee_report->GetStatsOfType<RTCTransportStats>().size());
}
#endif  // HAVE_SCTP

TEST_P(PeerConnectionIntegrationTest,
       IceEventsGeneratedAndLoggedInRtcEventLog) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithFakeRtcEventLog());
  fixture.ConnectFakeSignaling();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  fixture.caller()->SetOfferAnswerOptions(options);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.DtlsConnected(), kDefaultTimeout);
  ASSERT_NE(nullptr, fixture.caller()->event_log_factory());
  ASSERT_NE(nullptr, fixture.callee()->event_log_factory());
  webrtc::FakeRtcEventLog* caller_event_log =
      static_cast<webrtc::FakeRtcEventLog*>(
          fixture.caller()->event_log_factory()->last_log_created());
  webrtc::FakeRtcEventLog* callee_event_log =
      static_cast<webrtc::FakeRtcEventLog*>(
          fixture.callee()->event_log_factory()->last_log_created());
  ASSERT_NE(nullptr, caller_event_log);
  ASSERT_NE(nullptr, callee_event_log);
  int caller_ice_config_count = caller_event_log->GetEventCount(
      webrtc::RtcEvent::Type::IceCandidatePairConfig);
  int caller_ice_event_count = caller_event_log->GetEventCount(
      webrtc::RtcEvent::Type::IceCandidatePairEvent);
  int callee_ice_config_count = callee_event_log->GetEventCount(
      webrtc::RtcEvent::Type::IceCandidatePairConfig);
  int callee_ice_event_count = callee_event_log->GetEventCount(
      webrtc::RtcEvent::Type::IceCandidatePairEvent);
  EXPECT_LT(0, caller_ice_config_count);
  EXPECT_LT(0, caller_ice_event_count);
  EXPECT_LT(0, callee_ice_config_count);
  EXPECT_LT(0, callee_ice_event_count);
}

TEST_P(PeerConnectionIntegrationTest, RegatherAfterChangingIceTransportType) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-GatherOnCandidateFilterChanged/Enabled/");
  // PeerConnections must be destroyed before ScopedFieldTrials.
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  fixture.CreateTurnServer(turn_server_internal_address,
                           turn_server_external_address);

  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:88.88.88.0:3478");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration caller_config;
  caller_config.servers.push_back(ice_server);
  caller_config.type = webrtc::PeerConnectionInterface::kRelay;
  caller_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;

  PeerConnectionInterface::RTCConfiguration callee_config;
  callee_config.servers.push_back(ice_server);
  callee_config.type = webrtc::PeerConnectionInterface::kRelay;
  callee_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;

  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(caller_config,
                                                             callee_config));

  // Do normal offer/answer and wait for ICE to complete.
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Since we are doing continual gathering, the ICE transport does not reach
  // kIceGatheringComplete (see
  // P2PTransportChannel::OnCandidatesAllocationDone), and consequently not
  // kIceConnectionComplete.
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 fixture.callee()->ice_connection_state(), kDefaultTimeout);
  // Note that we cannot use the metric
  // |WebRTC.PeerConnection.CandidatePairType_UDP| in this test since this
  // metric is only populated when we reach kIceConnectionComplete in the
  // current implementation.
  EXPECT_EQ(cricket::RELAY_PORT_TYPE,
            fixture.caller()->last_candidate_gathered().type());
  EXPECT_EQ(cricket::RELAY_PORT_TYPE,
            fixture.callee()->last_candidate_gathered().type());

  // Loosen the caller's candidate filter.
  caller_config = fixture.caller()->pc()->GetConfiguration();
  caller_config.type = webrtc::PeerConnectionInterface::kAll;
  fixture.caller()->pc()->SetConfiguration(caller_config);
  // We should have gathered a new host candidate.
  EXPECT_EQ_WAIT(cricket::LOCAL_PORT_TYPE,
                 fixture.caller()->last_candidate_gathered().type(),
                 kDefaultTimeout);

  // Loosen the callee's candidate filter.
  callee_config = fixture.callee()->pc()->GetConfiguration();
  callee_config.type = webrtc::PeerConnectionInterface::kAll;
  fixture.callee()->pc()->SetConfiguration(callee_config);
  EXPECT_EQ_WAIT(cricket::LOCAL_PORT_TYPE,
                 fixture.callee()->last_candidate_gathered().type(),
                 kDefaultTimeout);
}

INSTANTIATE_TEST_SUITE_P(PeerConnectionIntegrationTest,
                         PeerConnectionIntegrationTest,
                         Values(SdpSemantics::kPlanB,
                                SdpSemantics::kUnifiedPlan));

class PeerConnectionIntegrationInteropTestFixture
    : public PeerConnectionIntegrationTestFixture {
 public:
  // Setting the SdpSemantics for the base test to kDefault does not matter
  // because we specify not to use the test semantics when creating
  // PeerConnectionWrappers.
  PeerConnectionIntegrationInteropTestFixture()
      : PeerConnectionIntegrationTestFixture(SdpSemantics::kPlanB) {}

  bool CreatePeerConnectionWrappersWithSemantics(
      SdpSemantics caller_semantics,
      SdpSemantics callee_semantics) {
    return CreatePeerConnectionWrappersWithSdpSemantics(caller_semantics,
                                                        callee_semantics);
  }

 private:
  std::unique_ptr<cricket::TestStunServer> stun_server_;
};

// Tests that verify interoperability between Plan B and Unified Plan
// PeerConnections.
class PeerConnectionIntegrationInteropTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, SdpSemantics>> {
 protected:
  SdpSemantics GetCallerSemantics() const { return std::get<0>(GetParam()); }
  SdpSemantics GetCalleeSemantics() const { return std::get<1>(GetParam()); }
};

TEST_P(PeerConnectionIntegrationInteropTest, NoMediaLocalToNoMediaRemote) {
  PeerConnectionIntegrationInteropTestFixture fixture;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithSemantics(
      GetCallerSemantics(), GetCalleeSemantics()));
  fixture.ConnectFakeSignaling();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationInteropTest, OneAudioLocalToNoMediaRemote) {
  PeerConnectionIntegrationInteropTestFixture fixture;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithSemantics(
      GetCallerSemantics(), GetCalleeSemantics()));
  fixture.ConnectFakeSignaling();
  auto audio_sender = fixture.caller()->AddAudioTrack();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Verify that one audio receiver has been created on the remote and that it
  // has the same track ID as the sending track.
  auto receivers = fixture.callee()->pc()->GetReceivers();
  ASSERT_EQ(1u, receivers.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, receivers[0]->media_type());
  EXPECT_EQ(receivers[0]->track()->id(), audio_sender->track()->id());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationInteropTest, OneAudioOneVideoToNoMediaRemote) {
  PeerConnectionIntegrationInteropTestFixture fixture;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithSemantics(
      GetCallerSemantics(), GetCalleeSemantics()));
  fixture.ConnectFakeSignaling();
  auto video_sender = fixture.caller()->AddVideoTrack();
  auto audio_sender = fixture.caller()->AddAudioTrack();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Verify that one audio and one video receiver have been created on the
  // remote and that they have the same track IDs as the sending tracks.
  auto audio_receivers =
      fixture.callee()->GetReceiversOfType(cricket::MEDIA_TYPE_AUDIO);
  ASSERT_EQ(1u, audio_receivers.size());
  EXPECT_EQ(audio_receivers[0]->track()->id(), audio_sender->track()->id());
  auto video_receivers =
      fixture.callee()->GetReceiversOfType(cricket::MEDIA_TYPE_VIDEO);
  ASSERT_EQ(1u, video_receivers.size());
  EXPECT_EQ(video_receivers[0]->track()->id(), video_sender->track()->id());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationInteropTest,
       OneAudioOneVideoLocalToOneAudioOneVideoRemote) {
  PeerConnectionIntegrationInteropTestFixture fixture;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithSemantics(
      GetCallerSemantics(), GetCalleeSemantics()));
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationInteropTest,
       ReverseRolesOneAudioLocalToOneVideoRemote) {
  PeerConnectionIntegrationInteropTestFixture fixture;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithSemantics(
      GetCallerSemantics(), GetCalleeSemantics()));
  fixture.ConnectFakeSignaling();
  fixture.caller()->AddAudioTrack();
  fixture.callee()->AddVideoTrack();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Verify that only the audio track has been negotiated.
  EXPECT_EQ(
      0u,
      fixture.caller()->GetReceiversOfType(cricket::MEDIA_TYPE_VIDEO).size());
  // Might also check that the callee's NegotiationNeeded flag is set.

  // Reverse roles.
  fixture.callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CallerExpectsSomeVideo();
  media_expectations.CalleeExpectsSomeAudio();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionIntegrationTest,
    PeerConnectionIntegrationInteropTest,
    Values(std::make_tuple(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
           std::make_tuple(SdpSemantics::kUnifiedPlan, SdpSemantics::kPlanB)));

// Test that if the Unified Plan side offers two video tracks then the Plan B
// side will only see the first one and ignore the second.
TEST_F(PeerConnectionIntegrationTestPlanB, TwoVideoUnifiedPlanToNoMediaPlanB) {
  PeerConnectionIntegrationInteropTestFixture fixture;
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithSdpSemantics(
      SdpSemantics::kUnifiedPlan, SdpSemantics::kPlanB));
  fixture.ConnectFakeSignaling();
  auto first_sender = fixture.caller()->AddVideoTrack();
  fixture.caller()->AddVideoTrack();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);

  // Verify that there is only one receiver and it corresponds to the first
  // added track.
  auto receivers = fixture.callee()->pc()->GetReceivers();
  ASSERT_EQ(1u, receivers.size());
  EXPECT_TRUE(receivers[0]->track()->enabled());
  EXPECT_EQ(first_sender->track()->id(), receivers[0]->track()->id());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
}

// Test that if the initial offer tagged BUNDLE section is rejected due to its
// associated RtpTransceiver being stopped and another transceiver is added,
// then renegotiation causes the callee to receive the new video track without
// error.
// This is a regression test for bugs.webrtc.org/9954
TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       ReOfferWithStoppedBundleTaggedTransceiver) {
  RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappersWithConfig(config, config));
  fixture.ConnectFakeSignaling();
  auto audio_transceiver_or_error = fixture.caller()->pc()->AddTransceiver(
      fixture.caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto audio_transceiver = audio_transceiver_or_error.MoveValue();

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }

  audio_transceiver->Stop();
  fixture.caller()->pc()->AddTransceiver(
      fixture.caller()->CreateLocalVideoTrack());

  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeVideo();
    ASSERT_TRUE(fixture.ExpectNewFrames(media_expectations));
  }
}

#ifdef HAVE_SCTP

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithBundledSctpDataChannel) {
  PeerConnectionIntegrationTestFixture fixture(GetSdpSemantics());
  ASSERT_TRUE(fixture.CreatePeerConnectionWrappers());
  fixture.ConnectFakeSignaling();
  fixture.caller()->CreateDataChannel();
  fixture.caller()->AddAudioVideoTracks();
  fixture.callee()->AddAudioVideoTracks();
  fixture.caller()->SetGeneratedSdpMunger(MakeSpecCompliantSctpOffer);
  fixture.caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(fixture.SignalingStateStable(), kDefaultTimeout);
  // Ensure that media and data are multiplexed on the same DTLS transport.
  // This only works on Unified Plan, because transports are not exposed in plan
  // B.
  auto sctp_info = fixture.caller()->pc()->GetSctpTransport()->Information();
  EXPECT_EQ(sctp_info.dtls_transport(),
            fixture.caller()->pc()->GetSenders()[0]->dtls_transport());
}

#endif  // HAVE_SCTP

}  // namespace
}  // namespace webrtc

#endif  // if !defined(THREAD_SANITIZER)
