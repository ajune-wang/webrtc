/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtlstransport.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "p2p/base/fakedtlstransport.h"
#include "rtc_base/gunit.h"
#include "test/gtest.h"

constexpr int kDefaultTimeout = 1000;  // milliseconds

using cricket::FakeDtlsTransport;

namespace webrtc {

class TestDtlsTransportObserver : public DtlsTransportObserverInterface {
 public:
  void OnStateChange(DtlsTransportState state,
                     bool state_changed,
                     void* remoteCertificates) override {
    state_change_called_ = true;
    states_.push_back(state);
  }

  void OnError(RTCError error) override {}

  bool state_change_called_ = false;
  std::vector<DtlsTransportState> states_;
};

class DtlsTransportTest : public testing::Test {
 public:
  DtlsTransport* transport() { return transport_.get(); }
  DtlsTransportObserverInterface* observer() { return observer_.get(); }

  void CreateTransport() {
    auto cricket_transport = absl::make_unique<FakeDtlsTransport>(
        "audio", cricket::ICE_CANDIDATE_COMPONENT_RTP);
    transport_ =
        new rtc::RefCountedObject<DtlsTransport>(std::move(cricket_transport));
  }

  void CreateObserver() {
    observer_ = absl::make_unique<TestDtlsTransportObserver>();
  }

  void CompleteDtlsHandshake() {
    auto fake_dtls1 = static_cast<FakeDtlsTransport*>(transport_->internal());
    auto fake_dtls2 = absl::make_unique<FakeDtlsTransport>(
        "audio", cricket::ICE_CANDIDATE_COMPONENT_RTP);
    auto cert1 = rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
        rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
    fake_dtls1->SetLocalCertificate(cert1);
    auto cert2 = rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
        rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
    fake_dtls2->SetLocalCertificate(cert2);
    fake_dtls1->SetDestination(fake_dtls2.get());
  }

  rtc::scoped_refptr<DtlsTransport> transport_;
  std::unique_ptr<TestDtlsTransportObserver> observer_;
};

TEST_F(DtlsTransportTest, CreateClearDelete) {
  auto cricket_transport = absl::make_unique<FakeDtlsTransport>(
      "audio", cricket::ICE_CANDIDATE_COMPONENT_RTP);
  auto webrtc_transport =
      new rtc::RefCountedObject<DtlsTransport>(std::move(cricket_transport));
  ASSERT_TRUE(webrtc_transport->internal());
  webrtc_transport->clear();
  ASSERT_FALSE(webrtc_transport->internal());
}

TEST_F(DtlsTransportTest, ObserverSendsEventWhenInstalled) {
  CreateTransport();
  CreateObserver();
  transport()->RegisterObserver(observer());
  ASSERT_TRUE_WAIT(observer_->state_change_called_, kDefaultTimeout);
  EXPECT_EQ(std::vector<DtlsTransportState>({DtlsTransportState::kNew}),
            observer_->states_);
}

TEST_F(DtlsTransportTest, EventsObservedWhenConnecting) {
  CreateTransport();
  CreateObserver();
  transport()->RegisterObserver(observer());
  CompleteDtlsHandshake();
  ASSERT_TRUE_WAIT(observer_->state_change_called_, kDefaultTimeout);
  EXPECT_EQ(std::vector<DtlsTransportState>(
                {DtlsTransportState::kNew,
                 // FakeDtlsTransport doesn't signal the "connecting" state.
                 // TODO(hta): fix FakeDtlsTransport or file bug on it.
                 // DtlsTransportState::kConnecting,
                 DtlsTransportState::kConnected}),
            observer_->states_);
}

}  // namespace webrtc
