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
#include "rtc_base/bind.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class FakePeriodicVideoCapturer : public cricket::FakeVideoCapturer {
 public:
  FakePeriodicVideoCapturer() {
    worker_thread_checker_.DetachFromThread();
    std::vector<cricket::VideoFormat> formats;
    formats.push_back(cricket::VideoFormat(1280, 720,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 360,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(320, 240,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(160, 120,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    ResetSupportedFormats(formats);
  }

  ~FakePeriodicVideoCapturer() override {
    RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(!task_queue_) << "Stop hasn't been called.";
  }

  // Workaround method for tests to allow stopping frame delivery directly.
  // The worker thread thread, on which Start() is called, is not accessible via
  // OrtcFactoryInterface, nor is it injectable.
  //
  // WARNING: It is assumed that the caller has a way of knowing that Start()
  // has successfully been called before calling this method and furthermore
  // guarantees that Stop() will not be called via other means while this method
  // is being called.
  void StopFrameDeliveryForTesting() {
    RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(worker_thread_);
    worker_thread_->Invoke<void>(
        RTC_FROM_HERE, rtc::Bind(&cricket::VideoCapturer::Stop,
                                 static_cast<cricket::VideoCapturer*>(this)));
    RTC_DCHECK(!task_queue_) << "task_queue_ expected to have been deleted";
    RTC_DCHECK(!worker_thread_);
  }

 private:
  cricket::CaptureState Start(const cricket::VideoFormat& format) override {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(!worker_thread_);
    RTC_DCHECK(!task_queue_);
    task_queue_.reset(new rtc::TaskQueue("FakePeriodicVideoCapturer"));
    worker_thread_ = rtc::Thread::Current();
    cricket::CaptureState state = FakeVideoCapturer::Start(format);
    if (state != cricket::CS_FAILED) {
      task_queue_->PostTask([this]() { DeliverFrame(); });
    }
    return state;
  }

  void Stop() override {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (!worker_thread_)
      return;

    RTC_DCHECK_EQ(worker_thread_, rtc::Thread::Current());
    worker_thread_ = nullptr;
    task_queue_ = nullptr;
  }

  void DeliverFrame() {
    RTC_DCHECK(task_queue_->IsCurrent());
    if (IsRunning()) {
      CaptureFrame();
      task_queue_->PostDelayedTask(
          [this]() { DeliverFrame(); },
          static_cast<int>(GetCaptureFormat()->interval /
                           rtc::kNumNanosecsPerMillisec));
    }
  }

  rtc::ThreadChecker main_thread_checker_;
  rtc::ThreadChecker worker_thread_checker_;
  rtc::Thread* worker_thread_ = nullptr;
  std::unique_ptr<rtc::TaskQueue> task_queue_;
};

}  // namespace webrtc

#endif  //  PC_TEST_FAKEPERIODICVIDEOCAPTURER_H_
