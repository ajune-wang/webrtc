/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// FakePeriodicVideoCapturer implements a fake cricket::VideoCapturer that
// creates video frames periodically after it has been started.

#ifndef PC_TEST_FAKEPERIODICVIDEOCAPTURER_H_
#define PC_TEST_FAKEPERIODICVIDEOCAPTURER_H_

#include <memory>
#include <vector>

#include "media/base/fakevideocapturer.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class FakePeriodicVideoCapturer : public cricket::FakeVideoCapturer {
 public:
  FakePeriodicVideoCapturer() {
    worker_thread_checker_.DetachFromThread();
    using cricket::VideoFormat;
    static const VideoFormat formats[] = {
        {1280, 720, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
        {640, 480, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
        {640, 360, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
        {320, 240, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
        {160, 120, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
    };

    ResetSupportedFormats({&formats[0], &formats[arraysize(formats) - 1]});
  }

  ~FakePeriodicVideoCapturer() override {
    RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
  }

  // Workaround method for tests to allow stopping frame delivery directly.
  // The worker thread thread, on which Start() is called, is not accessible via
  // OrtcFactoryInterface, nor is it injectable. So, there isn't a convenient
  // way from the test to call Stop() directly (and correctly).
  void StopFrameDeliveryForTesting() {
    RTC_DCHECK(!task_queue_.IsCurrent());
    rtc::Event event(false, false);
    task_queue_.PostTask([this, &event]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      deliver_frames_ = false;
      event.Set();
    });
    event.Wait(rtc::Event::kForever);
  }

 private:
  cricket::CaptureState Start(const cricket::VideoFormat& format) override {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    cricket::CaptureState state = FakeVideoCapturer::Start(format);
    if (state != cricket::CS_FAILED) {
      task_queue_.PostTask([this]() {
        RTC_DCHECK_RUN_ON(&task_queue_);
        deliver_frames_ = true;
        DeliverFrame();
      });
    }
    return state;
  }

  void Stop() override {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    StopFrameDeliveryForTesting();
  }

  void DeliverFrame() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (IsRunning() && deliver_frames_) {
      CaptureFrame();
      if (periodic_delivery_) {
        task_queue_.PostDelayedTask(
            [this]() { DeliverFrame(); },
            static_cast<int>(GetCaptureFormat()->interval /
                             rtc::kNumNanosecsPerMillisec));
      }
    }
  }

  rtc::ThreadChecker main_thread_checker_;
  rtc::ThreadChecker worker_thread_checker_;
  rtc::TaskQueue task_queue_{"FakePeriodicVideoCapturer"};
  bool deliver_frames_ RTC_ACCESS_ON(task_queue_) = false;
  bool periodic_delivery_ RTC_ACCESS_ON(task_queue_) = true;
};

}  // namespace webrtc

#endif  //  PC_TEST_FAKEPERIODICVIDEOCAPTURER_H_
