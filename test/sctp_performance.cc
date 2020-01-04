/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "test/gtest.h"

namespace webrtc {

class FunctionalCreateSessionDescriptionObserver
    : public CreateSessionDescriptionObserver {
 public:
  static rtc::scoped_refptr<CreateSessionDescriptionObserver> Create(
      std::function<void(SessionDescriptionInterface*)> success_cb,
      std::function<void(RTCError)> failure_cb) {
    return new rtc::RefCountedObject<
        FunctionalCreateSessionDescriptionObserver>(std::move(success_cb),
                                                    std::move(failure_cb));
  }

  FunctionalCreateSessionDescriptionObserver(
      std::function<void(SessionDescriptionInterface*)> success_cb,
      std::function<void(RTCError)> failure_cb)
      : success_cb_(std::move(success_cb)),
        failure_cb_(std::move(failure_cb)) {}

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(SessionDescriptionInterface* session_description) override {
    if (success_cb_) {
      success_cb_(session_description);
    }
  }
  void OnFailure(RTCError error) override {
    if (failure_cb_) {
      failure_cb_(std::move(error));
    }
  }

 private:
  std::function<void(SessionDescriptionInterface*)> success_cb_;
  std::function<void(RTCError)> failure_cb_;
};

class FunctionalSetSessionDescriptionObserver
    : public SetSessionDescriptionObserver {
 public:
  static rtc::scoped_refptr<SetSessionDescriptionObserver> Create(
      std::function<void()> success_cb,
      std::function<void(RTCError)> failure_cb) {
    return new rtc::RefCountedObject<FunctionalSetSessionDescriptionObserver>(
        std::move(success_cb), std::move(failure_cb));
  }

  FunctionalSetSessionDescriptionObserver(
      std::function<void()> success_cb,
      std::function<void(RTCError)> failure_cb)
      : success_cb_(std::move(success_cb)),
        failure_cb_(std::move(failure_cb)) {}

  // SetSessionDescriptionObserver implementation.
  void OnSuccess() override {
    if (success_cb_) {
      success_cb_();
    }
  }
  void OnFailure(RTCError error) override {
    if (failure_cb_) {
      failure_cb_(std::move(error));
    }
  }

 private:
  std::function<void()> success_cb_;
  std::function<void(RTCError)> failure_cb_;
};

class PeerEndpoint : public PeerConnectionObserver {
 public:
  explicit PeerEndpoint(const std::string& prefix)
      : prefix_(prefix),
        application_thread_(rtc::Thread::Create()),
        signaling_thread_(rtc::Thread::Create()),
        network_thread_(rtc::Thread::CreateWithSocketServer()),
        worker_thread_(rtc::Thread::Create()) {
    application_thread_->SetName(prefix + "-app", nullptr);
    signaling_thread_->SetName(prefix + "-signaling", nullptr);
    network_thread_->SetName(prefix + "-network", nullptr);
    worker_thread_->SetName(prefix + "-network", nullptr);
  }

  void Start() {
    application_thread_->Start();
    application_thread_->Invoke<void>(RTC_FROM_HERE, [this]() {
      signaling_thread_->Start();
      network_thread_->Start();
      worker_thread_->Start();
      ConstructPeerConnection();
    });
  }

  void set_connected_cb(std::function<void()> cb) {
    connected_cb_ = std::move(cb);
  }

  void set_remote_data_channel_cb(
      std::function<void(rtc::scoped_refptr<DataChannelInterface>)> cb) {
    remote_data_channel_cb_ = std::move(cb);
  }

  void ConnectTo(PeerEndpoint* peer, std::function<void()> connected_cb) {
    RTC_DCHECK(peer);
    RTC_DCHECK_RUN_ON(application_thread());
    RTC_DCHECK(!peer_);

    peer_ = peer;
    connected_cb_ = connected_cb;
    role_ = Role::kOfferer;

    PeerConnectionInterface::RTCOfferAnswerOptions options;
    peer_connection_->CreateOffer(
        FunctionalCreateSessionDescriptionObserver::Create(
            [this](SessionDescriptionInterface* offer) {
              std::string offer_sdp;
              offer->ToString(&offer_sdp);
              SdpParseError error;
              std::unique_ptr<SessionDescriptionInterface> offer_copy =
                  CreateSessionDescription(SdpType::kOffer, offer_sdp, &error);
              peer_connection_->SetLocalDescription(
                  FunctionalSetSessionDescriptionObserver::Create(nullptr,
                                                                  nullptr),
                  offer_copy.release());
              peer_->application_thread()->PostTask(
                  RTC_FROM_HERE, [this, offer_sdp = std::move(offer_sdp)]() {
                    peer_->ConnectFrom(this, offer_sdp);
                  });
            },
            nullptr),
        options);
  }

  void ConnectFrom(PeerEndpoint* peer, const std::string& offer_sdp) {
    RTC_DCHECK(peer);
    RTC_DCHECK_RUN_ON(application_thread());
    RTC_DCHECK(!peer_);

    peer_ = peer;
    role_ = Role::kAnswerer;

    SdpParseError error;
    std::unique_ptr<SessionDescriptionInterface> offer =
        CreateSessionDescription(SdpType::kOffer, offer_sdp, &error);
    peer_connection_->SetRemoteDescription(
        FunctionalSetSessionDescriptionObserver::Create(
            [this]() {
              PeerConnectionInterface::RTCOfferAnswerOptions options;
              peer_connection_->CreateAnswer(
                  FunctionalCreateSessionDescriptionObserver::Create(
                      [this](SessionDescriptionInterface* answer) {
                        std::string answer_sdp;
                        answer->ToString(&answer_sdp);
                        SdpParseError error;
                        std::unique_ptr<SessionDescriptionInterface>
                            answer_copy = CreateSessionDescription(
                                SdpType::kAnswer, answer_sdp, &error);
                        peer_connection_->SetLocalDescription(
                            FunctionalSetSessionDescriptionObserver::Create(
                                nullptr, nullptr),
                            answer_copy.release());
                        peer_->application_thread()->PostTask(
                            RTC_FROM_HERE,
                            [this, answer_sdp = std::move(answer_sdp)]() {
                              SdpParseError error;
                              std::unique_ptr<SessionDescriptionInterface>
                                  answer = CreateSessionDescription(
                                      SdpType::kAnswer, answer_sdp, &error);
                              peer_->peer_connection_->SetRemoteDescription(
                                  FunctionalSetSessionDescriptionObserver::
                                      Create(nullptr, nullptr),
                                  answer.release());
                            });
                      },
                      nullptr),
                  options);
            },
            nullptr),
        offer.release());
  }

  rtc::Thread* application_thread() { return application_thread_.get(); }
  PeerConnectionInterface* peer_connection() { return peer_connection_; }

  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {}
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    RTC_DCHECK_RUN_ON(signaling_thread_.get());
    if (remote_data_channel_cb_) {
      remote_data_channel_cb_(std::move(data_channel));
    }
  }
  void OnRenegotiationNeeded() override {}
  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnStandardizedIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    RTC_DCHECK_RUN_ON(signaling_thread_.get());
    if (new_state == PeerConnectionInterface::kIceConnectionConnected) {
      if (connected_cb_) {
        connected_cb_();
      }
    }
  }
  void OnIceCandidate(const IceCandidateInterface* candidate) override {
    RTC_DCHECK_RUN_ON(signaling_thread_.get());
    RTC_LOG(LS_INFO) << "[" << prefix_ << "] gathered ICE candidate";
    RTC_DCHECK(peer_);
    std::string candidate_str;
    RTC_CHECK(candidate->ToString(&candidate_str));
    SdpParseError error;
    std::unique_ptr<IceCandidateInterface> candidate_clone(
        CreateIceCandidate(candidate->sdp_mid(), candidate->sdp_mline_index(),
                           candidate_str, &error));
    peer_->application_thread()->PostTask(
        RTC_FROM_HERE,
        [peer = peer_, candidate = std::move(candidate_clone)]() {
          RTC_CHECK(peer->peer_connection()->AddIceCandidate(candidate.get()));
        });
  }

 private:
  void ConstructPeerConnection() {
    RTC_DCHECK_RUN_ON(application_thread());
    PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = network_thread_.get();
    dependencies.worker_thread = worker_thread_.get();
    dependencies.signaling_thread = signaling_thread_.get();
    peer_connection_factory_ =
        CreateModularPeerConnectionFactory(std::move(dependencies));
    PeerConnectionInterface::RTCConfiguration configuration;
    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
        configuration, nullptr, nullptr, this);
  }

  enum class Role {
    kOfferer,
    kAnswerer,
  };

  const std::string prefix_;
  const std::unique_ptr<rtc::Thread> application_thread_;
  const std::unique_ptr<rtc::Thread> signaling_thread_;
  const std::unique_ptr<rtc::Thread> network_thread_;
  const std::unique_ptr<rtc::Thread> worker_thread_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> peer_connection_factory_;
  rtc::scoped_refptr<PeerConnectionInterface> peer_connection_;
  std::function<void()> connected_cb_;
  std::function<void(rtc::scoped_refptr<DataChannelInterface>)>
      remote_data_channel_cb_;
  PeerEndpoint* peer_ = nullptr;
  Role role_ = Role::kOfferer;
};

class ReceiverDataChannelObserver : public DataChannelObserver {
 public:
  ReceiverDataChannelObserver(
      rtc::scoped_refptr<DataChannelInterface> data_channel,
      std::function<void()> closed_cb)
      : data_channel_(std::move(data_channel)),
        closed_cb_(std::move(closed_cb)) {}

  // DataChannelObserver implementation.
  void OnStateChange() override {
    RTC_LOG(LS_INFO) << "Data channel state: "
                     << DataChannelInterface::DataStateString(
                            data_channel_->state());
    if (data_channel_->state() == DataChannelInterface::DataState::kClosed) {
      Report(Timestamp::us(rtc::TimeMicros()));
      if (closed_cb_) {
        closed_cb_();
      }
      data_channel_ = nullptr;
    }
  }
  void OnMessage(const DataBuffer& buffer) override {
    // RTC_LOG(LS_INFO) << "Received: " << buffer.size() << " bytes";
    bytes_received_ += buffer.size();
    Timestamp now = Timestamp::us(rtc::TimeMicros());
    if (last_report_.IsZero()) {
      last_report_ = now;
    } else if ((now - last_report_).ms() >= 500) {
      Report(now);
      last_bytes_received_ = bytes_received_;
      last_report_ = now;
    }
  }

 private:
  void Report(Timestamp now) {
    TimeDelta elapsed_time = now - last_report_;
    uint64_t elapsed_bytes = bytes_received_ - last_bytes_received_;
    double rate = (static_cast<double>(elapsed_bytes) / elapsed_time.us() * 8);
    RTC_LOG(LS_INFO) << "Receive rate: " << rate << " Mbps";
  }

  rtc::scoped_refptr<DataChannelInterface> data_channel_;
  std::function<void()> closed_cb_;
  uint64_t bytes_received_ = 0;
  uint64_t last_bytes_received_ = 0;
  Timestamp last_report_ = Timestamp::Zero();
};

class DataChannelSender : public DataChannelObserver {
 public:
  struct Config {
    size_t block_size;
    size_t num_blocks;
    size_t buffered_high_water_mark;
    size_t buffered_low_water_mark;
  };

  explicit DataChannelSender(Config config)
      : config_(std::move(config)), payload_(config.block_size) {
    for (size_t i = 0; i < payload_.size(); ++i) {
      payload_.data()[i] = i % 256;
    }
  }

  void Start(rtc::scoped_refptr<DataChannelInterface> data_channel) {
    RTC_DCHECK(data_channel);
    application_thread_ = rtc::Thread::Current();
    data_channel_ = std::move(data_channel);
    data_channel_->RegisterObserver(this);
    Run();
  }

  // DataChannelObserver implementation.
  void OnStateChange() override {}
  void OnMessage(const DataBuffer& buffer) override {}
  void OnBufferedAmountChange(uint64_t sent_data_size) override {
    if (data_channel_->buffered_amount() + sent_data_size >
            config_.buffered_low_water_mark &&
        data_channel_->buffered_amount() <= config_.buffered_low_water_mark) {
      application_thread_->PostTask(RTC_FROM_HERE, [this]() { Run(); });
    }
  }

 private:
  void Run() {
    while (block_ < config_.num_blocks) {
      if (data_channel_->buffered_amount() > config_.buffered_high_water_mark) {
        break;
      }
      RTC_CHECK(data_channel_->Send(DataBuffer(payload_, /*binary=*/true)));
      ++block_;
    }
    if (block_ == config_.num_blocks) {
      data_channel_->Close();
    }
  }

 private:
  Config config_;
  rtc::Thread* application_thread_;
  rtc::scoped_refptr<DataChannelInterface> data_channel_;
  rtc::CopyOnWriteBuffer payload_;
  size_t block_ = 0;
};

TEST(SctpPerformance, Performance) {
  PeerEndpoint sender("sender");
  PeerEndpoint receiver("receiver");

  sender.Start();
  receiver.Start();

  rtc::scoped_refptr<DataChannelInterface> sender_data_channel;
  rtc::Event receiver_data_channel_ready;
  rtc::scoped_refptr<DataChannelInterface> receiver_data_channel;

  receiver.application_thread()->Invoke<void>(RTC_FROM_HERE, [&]() {
    receiver.set_remote_data_channel_cb(
        [&receiver_data_channel, &receiver_data_channel_ready](
            rtc::scoped_refptr<DataChannelInterface> data_channel) {
          receiver_data_channel = std::move(data_channel);
          receiver_data_channel_ready.Set();
        });
  });

  rtc::Event sender_connected;
  sender.application_thread()->Invoke<void>(RTC_FROM_HERE, [&]() {
    sender_data_channel =
        sender.peer_connection()->CreateDataChannel("init", nullptr);
    sender.ConnectTo(&receiver,
                     [&sender_connected]() { sender_connected.Set(); });
  });

  sender_connected.Wait(rtc::Event::kForever);
  RTC_DCHECK(sender_data_channel);

  receiver_data_channel_ready.Wait(rtc::Event::kForever);
  RTC_DCHECK(receiver_data_channel);

  rtc::Event closed_event;
  ReceiverDataChannelObserver receiver_observer(
      receiver_data_channel, [&closed_event]() { closed_event.Set(); });
  receiver.application_thread()->Invoke<void>(
      RTC_FROM_HERE,
      [&, receiver_data_channel = std::move(receiver_data_channel)]() {
        receiver_data_channel->RegisterObserver(&receiver_observer);
      });

  DataChannelSender::Config sender_config;
  sender_config.block_size = 1024;
  sender_config.num_blocks = 128 * 1024 * 1024 / sender_config.block_size;
  sender_config.buffered_high_water_mark = 128 * 1024;
  sender_config.buffered_low_water_mark =
      sender_config.buffered_high_water_mark / 2;
  DataChannelSender data_channel_sender(std::move(sender_config));
  sender.application_thread()->PostTask(
      RTC_FROM_HERE, [&data_channel_sender,
                      sender_data_channel = std::move(sender_data_channel)]() {
        data_channel_sender.Start(std::move(sender_data_channel));
      });

  closed_event.Wait(rtc::Event::kForever);
}

}  // namespace webrtc
