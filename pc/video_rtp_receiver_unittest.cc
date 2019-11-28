/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_receiver.h"

#include <memory>

#include "api/video/test/mock_recordable_encoded_frame.h"
#include "media/base/fake_media_engine.h"
#include "test/gmock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace webrtc {
namespace {

class VideoRtpReceiverTest : public testing::Test {
 protected:
  class MockVideoMediaChannel : public cricket::FakeVideoMediaChannel {
   public:
    MockVideoMediaChannel(cricket::FakeVideoEngine* engine,
                          const cricket::VideoOptions& options)
        : FakeVideoMediaChannel(engine, options) {}
    MOCK_METHOD2(SetEncodedFrameBufferFunction,
                 void(uint32_t, RecordableEncodedFrameFunction));
    MOCK_METHOD1(ClearEncodedFrameBufferFunction, void(uint32_t));
    MOCK_METHOD1(GenerateKeyFrame, void(uint32_t));
  };

  class MockVideoSink : public rtc::VideoSinkInterface<RecordableEncodedFrame> {
   public:
    MOCK_METHOD1(OnFrame, void(const RecordableEncodedFrame&));
  };

  VideoRtpReceiverTest()
      : worker_thread_(rtc::Thread::Create()),
        receiver_(
            new VideoRtpReceiver(worker_thread_.get(), "receiver", {"stream"})),
        channel_(nullptr, cricket::VideoOptions()) {
    worker_thread_->Start();
    receiver_->SetMediaChannel(&channel_);
  }

  ~VideoRtpReceiverTest() { receiver_->Stop(); }

  webrtc::VideoTrackSourceInterface* Source() {
    return receiver_->streams()[0]->FindVideoTrack("receiver")->GetSource();
  }

  void DepleteWorkOnWorkerThread() {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [] {});
  }

  std::unique_ptr<rtc::Thread> worker_thread_;
  rtc::scoped_refptr<VideoRtpReceiver> receiver_;
  MockVideoMediaChannel channel_;
};

TEST_F(VideoRtpReceiverTest, SupportsEncodedOutput) {
  EXPECT_TRUE(Source()->SupportsEncodedOutput());
}

TEST_F(VideoRtpReceiverTest, GeneratesKeyFrame) {
  EXPECT_CALL(channel_, GenerateKeyFrame(0));
  Source()->GenerateKeyFrame();
  DepleteWorkOnWorkerThread();
}

TEST_F(VideoRtpReceiverTest,
       GenerateKeyFrameOnChannelSwitchUnlessGenerateKeyframeCalled) {
  // A channel switch without previous call to GenerateKeyFrame shouldn't
  // cause a call to happen on the new channel.
  MockVideoMediaChannel channel2(nullptr, cricket::VideoOptions());
  EXPECT_CALL(channel_, GenerateKeyFrame).Times(0);
  EXPECT_CALL(channel2, GenerateKeyFrame).Times(0);
  receiver_->SetMediaChannel(&channel2);
  DepleteWorkOnWorkerThread();
  Mock::VerifyAndClearExpectations(&channel2);

  // Generate a key frame. When we switch channel next time, we will have to
  // re-generate it as we don't know if it was eventually received
  Source()->GenerateKeyFrame();
  DepleteWorkOnWorkerThread();
  MockVideoMediaChannel channel3(nullptr, cricket::VideoOptions());
  EXPECT_CALL(channel3, GenerateKeyFrame);
  receiver_->SetMediaChannel(&channel3);
  DepleteWorkOnWorkerThread();

  // Switching to a new channel should now not cause calls to GenerateKeyFrame.
  StrictMock<MockVideoMediaChannel> channel4(nullptr, cricket::VideoOptions());
  receiver_->SetMediaChannel(&channel4);
  DepleteWorkOnWorkerThread();
}

TEST_F(VideoRtpReceiverTest, EnablesEncodedOutput) {
  EXPECT_CALL(channel_, SetEncodedFrameBufferFunction(0, _));
  EXPECT_CALL(channel_, ClearEncodedFrameBufferFunction).Times(0);
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  DepleteWorkOnWorkerThread();
}

TEST_F(VideoRtpReceiverTest, DisablesEncodedOutput) {
  EXPECT_CALL(channel_, ClearEncodedFrameBufferFunction(0));
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  Source()->RemoveEncodedSink(&sink);
  DepleteWorkOnWorkerThread();
}

TEST_F(VideoRtpReceiverTest, DisablesEnablesEncodedOutputOnChannelSwitch) {
  InSequence s;
  EXPECT_CALL(channel_, SetEncodedFrameBufferFunction);
  EXPECT_CALL(channel_, ClearEncodedFrameBufferFunction);
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  DepleteWorkOnWorkerThread();
  MockVideoMediaChannel channel2(nullptr, cricket::VideoOptions());
  EXPECT_CALL(channel2, SetEncodedFrameBufferFunction);
  receiver_->SetMediaChannel(&channel2);
  DepleteWorkOnWorkerThread();
  Mock::VerifyAndClearExpectations(&channel2);

  // When clearing encoded frame buffer function, we need channel switches
  // to NOT set the callback again.
  EXPECT_CALL(channel2, ClearEncodedFrameBufferFunction);
  Source()->RemoveEncodedSink(&sink);
  DepleteWorkOnWorkerThread();
  StrictMock<MockVideoMediaChannel> channel3(nullptr, cricket::VideoOptions());
  receiver_->SetMediaChannel(&channel3);
  DepleteWorkOnWorkerThread();
}

TEST_F(VideoRtpReceiverTest, BroadcastsEncodedFramesWhenEnabled) {
  std::function<void(const RecordableEncodedFrame&)> broadcast;
  EXPECT_CALL(channel_, SetEncodedFrameBufferFunction(_, _))
      .WillRepeatedly(SaveArg<1>(&broadcast));
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  MockRecordableEncodedFrame frame;

  // Make sure SetEncodedFrameBufferFunction completes.
  DepleteWorkOnWorkerThread();
  Mock::VerifyAndClearExpectations(&channel_);

  // Pass two frames on different contexts.
  EXPECT_CALL(sink, OnFrame).Times(2);
  broadcast(frame);
  worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] { broadcast(frame); });
}

TEST_F(VideoRtpReceiverTest, EnablesEncodedOutputOnChannelRestart) {
  InSequence s;
  EXPECT_CALL(channel_, ClearEncodedFrameBufferFunction(0));
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  EXPECT_CALL(channel_, SetEncodedFrameBufferFunction(4711, _));
  receiver_->SetupMediaChannel(4711);
  DepleteWorkOnWorkerThread();
  EXPECT_CALL(channel_, ClearEncodedFrameBufferFunction(4711));
  EXPECT_CALL(channel_, SetEncodedFrameBufferFunction(0, _));
  receiver_->SetupUnsignaledMediaChannel();
  DepleteWorkOnWorkerThread();
}

}  // namespace
}  // namespace webrtc
