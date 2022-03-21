/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"

#include <memory>
#include <utility>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/linux/wayland/test_support/fake_screencast_stream.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Ge;

namespace webrtc {

class PipeWireStreamTest : public ::testing::Test,
                           public FakeScreenCastStream::StreamNotifier,
                           public SharedScreenCastStream::StreamNotifier {
 public:
  PipeWireStreamTest()
      : fake_screencast_stream_(std::make_unique<FakeScreenCastStream>(this)),
        shared_screencast_stream_(
            rtc::scoped_refptr(new SharedScreenCastStream(this))) {}

  ~PipeWireStreamTest() override {}

  // FakeScreenCastPortal::StreamNotifier
  MOCK_METHOD(void, OnFrameRecorded, (), (override));

  MOCK_METHOD(void, OnStreamReadyPtr, (int32_t stream_node_id));
  void OnStreamReady(uint32_t stream_node_id) final {
    OnStreamReadyPtr(static_cast<int32_t>(stream_node_id));
    shared_screencast_stream_->StartScreenCastStream(stream_node_id);
  }

  MOCK_METHOD(void, OnStartStreamingPtr, ());
  void OnStartStreaming() final {
    streaming_ = true;
    OnStartStreamingPtr();
  }

  MOCK_METHOD(void, OnStopStreamingPtr, ());
  void OnStopStreaming() final {
    streaming_ = false;
    OnStopStreamingPtr();
  }

  // SharedScreenCastStream::StreamNotifier
  MOCK_METHOD(void, OnCursorPositionChanged, (), (override));
  MOCK_METHOD(void, OnCursorShapeChanged, (), (override));
  MOCK_METHOD(void, OnDesktopFrameChanged, (), (override));
  MOCK_METHOD(void, OnFailedToProcessBuffer, (), (override));

 protected:
  uint recorded_frames_ = 0;
  bool streaming_ = false;
  std::unique_ptr<FakeScreenCastStream> fake_screencast_stream_;
  rtc::scoped_refptr<SharedScreenCastStream> shared_screencast_stream_;
};

TEST_F(PipeWireStreamTest, TestPipeWire) {
  // Step 1) Wait for FakePipeWireStream to be in ready state, providing us a
  // PipeWire node ID we can connect to
  rtc::Event streamReadyEvent;
  bool streamReady = false;
  EXPECT_CALL(*this, OnStreamReadyPtr(Ge(0)))
      .WillOnce([&streamReadyEvent, &streamReady] {
        streamReadyEvent.Set();
        streamReady = true;
      });
  streamReadyEvent.Wait(5000);
  EXPECT_TRUE(streamReady);

  // Step 2) Wait for FakePipeWireStream to start streaming, this means our
  // SharedScreenCastStream successfully connected to FakePipeWireStream
  rtc::Event startStreamingEvent;
  bool startedStreaming = false;
  EXPECT_CALL(*this, OnStartStreamingPtr())
      .WillOnce([&startStreamingEvent, &startedStreaming] {
        startStreamingEvent.Set();
        startedStreaming = true;
      });
  startStreamingEvent.Wait(5000);
  EXPECT_TRUE(startedStreaming);
  EXPECT_TRUE(streaming_);

  // Rather connect notifications earlier to be sure we don't miss the calls
  rtc::Event frameRecordedEvent;
  bool frameRecorded = false;
  EXPECT_CALL(*this, OnFrameRecorded())
      .WillRepeatedly([&frameRecordedEvent, &frameRecorded] {
        frameRecordedEvent.Set();
        frameRecorded = true;
      });

  rtc::Event frameRetrievedEvent;
  bool frameRetrieved = false;
  EXPECT_CALL(*this, OnDesktopFrameChanged())
      .WillRepeatedly([&frameRetrievedEvent, &frameRetrieved] {
        frameRetrievedEvent.Set();
        frameRetrieved = true;
      });

  // Step 3) Record a frame in FakePipeWireStream
  fake_screencast_stream_->RecordFrame();
  frameRecordedEvent.Wait(5000);
  EXPECT_TRUE(frameRecorded);

  // Step 4) Retrieve a frame from SharedScreenCastStream
  frameRetrievedEvent.Wait(5000);
  EXPECT_TRUE(frameRetrieved);

  // Check frame parameters
  std::unique_ptr<SharedDesktopFrame> frame =
      shared_screencast_stream_->CaptureFrame();
  ASSERT_NE(frame, nullptr);
  ASSERT_NE(frame->data(), nullptr);
  EXPECT_EQ(frame->rect().width(), 800);
  EXPECT_EQ(frame->rect().height(), 640);
  EXPECT_EQ(frame->stride(), frame->rect().width() * 4);

  // Test DesktopFrameQueue
  rtc::Event frameRetrievedEvent2;
  bool frameRetrieved2 = false;
  EXPECT_CALL(*this, OnDesktopFrameChanged())
      .WillRepeatedly([&frameRetrievedEvent2, &frameRetrieved2] {
        frameRetrievedEvent2.Set();
        frameRetrieved2 = true;
      });
  fake_screencast_stream_->RecordFrame();
  frameRetrievedEvent2.Wait(5000);
  EXPECT_TRUE(frameRetrieved2);
  std::unique_ptr<SharedDesktopFrame> frame2 =
      shared_screencast_stream_->CaptureFrame();
  ASSERT_NE(frame2, nullptr);
  ASSERT_NE(frame2->data(), nullptr);
  EXPECT_EQ(frame2->rect().width(), 800);
  EXPECT_EQ(frame2->rect().height(), 640);
  EXPECT_EQ(frame2->stride(), frame->rect().width() * 4);
  // Thanks to DesktopFrameQueue we should be able to have two frames shared
  EXPECT_EQ(frame->IsShared(), true);
  EXPECT_EQ(frame2->IsShared(), true);
  EXPECT_NE(frame->data(), frame2->data());
  // Third frame should not be recorded when both frames are shared
  rtc::Event bufferFailedEvent;
  bool bufferFailed = false;
  EXPECT_CALL(*this, OnFailedToProcessBuffer())
      .WillOnce([&bufferFailedEvent, &bufferFailed] {
        bufferFailedEvent.Set();
        bufferFailed = true;
      });
  fake_screencast_stream_->RecordFrame();
  bufferFailedEvent.Wait(5000);
  EXPECT_TRUE(bufferFailed);

  // Step 5) Disconnect from stream
  rtc::Event stopStreamingEvent;
  bool streamStopped = false;
  EXPECT_CALL(*this, OnStopStreamingPtr())
      .WillOnce([&stopStreamingEvent, &streamStopped] {
        stopStreamingEvent.Set();
        streamStopped = true;
      });
  shared_screencast_stream_->StopScreenCastStream();
  stopStreamingEvent.Wait(5000);
  EXPECT_TRUE(streamStopped);
  EXPECT_FALSE(streaming_);
}

}  // namespace webrtc
