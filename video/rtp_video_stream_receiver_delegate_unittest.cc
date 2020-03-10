/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/call/transport.h"
#include "call/video_receive_stream.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/logging.h"
#include "rtc_base/weak_ptr.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/rtp_video_stream_receiver.h"

using ::testing::_;

namespace webrtc {

namespace {

class FakeTransport : public Transport {
 public:
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) {
    return true;
  }
  bool SendRtcp(const uint8_t* packet, size_t length) { return true; }
};

class FakeNackSender : public NackSender {
 public:
  void SendNack(const std::vector<uint16_t>& sequence_numbers) {}
  void SendNack(const std::vector<uint16_t>& sequence_numbers,
                bool buffering_allowed) {}
};

class FakeOnCompleteFrameCallback
    : public video_coding::OnCompleteFrameCallback {
 public:
  void OnCompleteFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override {}
};

class MockFrameTransformer : public FrameTransformerInterface {
 public:
  MOCK_METHOD3(DoTransformFrame,
               void(std::unique_ptr<video_coding::EncodedFrame> frame,
                    std::vector<uint8_t> additional_data,
                    uint32_t ssrc));
  MOCK_METHOD1(DoRegisterTransformedFrameCallback,
               void(rtc::scoped_refptr<TransformedFrameCallback>));
  MOCK_METHOD0(DoUnregisterTransformedFrameCallback, void());

  void TransformFrame(std::unique_ptr<video_coding::EncodedFrame> frame,
                      std::vector<uint8_t> additional_data,
                      uint32_t ssrc) {
    DoTransformFrame(nullptr, additional_data, ssrc);
    auto transformed_frame = absl::WrapUnique(
        static_cast<video_coding::RtpFrameObject*>(frame.release()));
    callback_->OnTransformedFrame(std::move(transformed_frame));
  }

  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback> callback) {
    callback_ = callback;
    DoRegisterTransformedFrameCallback(std::move(callback));
  }

  void UnregisterTransformedFrameCallback() {
    callback_ = nullptr;
    DoUnregisterTransformedFrameCallback();
  }

 private:
  rtc::scoped_refptr<TransformedFrameCallback> callback_;
};

class TestRtpVideoStreamReceiverInitializer {
 public:
  TestRtpVideoStreamReceiverInitializer()
      : test_config_(nullptr),
        test_process_thread_(ProcessThread::Create("TestThread")) {
    test_config_.rtp.remote_ssrc = 1111;
    test_config_.rtp.local_ssrc = 2222;
    RTC_LOG(LS_ERROR) << "Ctor.";
    test_rtp_receive_statistics_ =
        ReceiveStatistics::Create(Clock::GetRealTimeClock());
    RTC_LOG(LS_ERROR) << "Ctor done";
  }

 protected:
  VideoReceiveStream::Config test_config_;
  FakeTransport fake_transport_;
  FakeNackSender fake_nack_sender_;
  FakeOnCompleteFrameCallback fake_on_complete_frame_callback_;
  std::unique_ptr<ProcessThread> test_process_thread_;
  std::unique_ptr<ReceiveStatistics> test_rtp_receive_statistics_;
};

class TestRtpVideoStreamReceiver : public TestRtpVideoStreamReceiverInitializer,
                                   public RtpVideoStreamReceiver {
 public:
  TestRtpVideoStreamReceiver()
      : TestRtpVideoStreamReceiverInitializer(),
        RtpVideoStreamReceiver(Clock::GetRealTimeClock(),
                               &fake_transport_,
                               nullptr,
                               nullptr,
                               &test_config_,
                               test_rtp_receive_statistics_.get(),
                               nullptr,
                               test_process_thread_.get(),
                               &fake_nack_sender_,
                               nullptr,
                               &fake_on_complete_frame_callback_,
                               nullptr,
                               nullptr),
        weak_ptr_factory_(this) {}
  ~TestRtpVideoStreamReceiver() override {}

  rtc::WeakPtr<RtpVideoStreamReceiver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD1(ManageFrame,
               void(std::unique_ptr<video_coding::RtpFrameObject> frame));

 private:
  rtc::WeakPtrFactory<TestRtpVideoStreamReceiver> weak_ptr_factory_;
};
}  // namespace

class RtpVideoReceiverFrameTransformerDelegateTest : public ::testing::Test {
 public:
  RtpVideoReceiverFrameTransformerDelegateTest()
      : frame_transformer_(new rtc::RefCountedObject<MockFrameTransformer>()),
        receiver_(std::make_unique<TestRtpVideoStreamReceiver>()) {
    delegate_ =
        new rtc::RefCountedObject<RtpVideoReceiverFrameTransformerDelegate>(
            receiver_->GetWeakPtr(), frame_transformer_);
    // RTC_LOG(LS_ERROR) << "Ctor done";
  }

 protected:
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer_;
  std::unique_ptr<TestRtpVideoStreamReceiver> receiver_;
  rtc::scoped_refptr<RtpVideoReceiverFrameTransformerDelegate> delegate_;
};

TEST_F(RtpVideoReceiverFrameTransformerDelegateTest, Init) {
  EXPECT_CALL(*frame_transformer_,
              DoRegisterTransformedFrameCallback(
                  rtc::scoped_refptr<TransformedFrameCallback>(delegate_)));
  delegate_->Init();
}

TEST_F(RtpVideoReceiverFrameTransformerDelegateTest, Reset) {
  EXPECT_CALL(*frame_transformer_, DoUnregisterTransformedFrameCallback());
  delegate_->Reset();
}

}  // namespace webrtc
