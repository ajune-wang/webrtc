/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <ApplicationServices/ApplicationServices.h>

#include <memory>
#include <ostream>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mock_desktop_capturer_callback.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::ResultOf;

namespace webrtc {

class ScreenCapturerMacTest : public ::testing::Test {
 public:
  // Verifies that the whole screen is initially dirty.
  void CaptureDoneCallback1(DesktopCapturer::Result result,
                            std::unique_ptr<DesktopFrame>* frame);

  // Verifies that a rectangle explicitly marked as dirty is propagated
  // correctly.
  void CaptureDoneCallback2(DesktopCapturer::Result result,
                            std::unique_ptr<DesktopFrame>* frame);

 protected:
  void SetUp() override {
    capturer_ = DesktopCapturer::CreateScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
  }

  std::unique_ptr<DesktopCapturer> capturer_;
  MockDesktopCapturerCallback callback_;
};

void ScreenCapturerMacTest::CaptureDoneCallback1(
    DesktopCapturer::Result result,
    std::unique_ptr<DesktopFrame>* frame) {
  EXPECT_EQ(result, DesktopCapturer::Result::SUCCESS);

  MacDesktopConfiguration config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::BottomLeftOrigin);

  // Verify that the region contains full frame.
  DesktopRegion::Iterator it((*frame)->updated_region());
  EXPECT_TRUE(!it.IsAtEnd() && it.rect().equals(config.pixel_bounds));
}

void ScreenCapturerMacTest::CaptureDoneCallback2(
    DesktopCapturer::Result result,
    std::unique_ptr<DesktopFrame>* frame) {
  EXPECT_EQ(result, DesktopCapturer::Result::SUCCESS);

  MacDesktopConfiguration config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::BottomLeftOrigin);
  int width = config.pixel_bounds.width();
  int height = config.pixel_bounds.height();

  EXPECT_EQ(width, (*frame)->size().width());
  EXPECT_EQ(height, (*frame)->size().height());
  EXPECT_TRUE((*frame)->data() != NULL);
  // Depending on the capture method, the screen may be flipped or not, so
  // the stride may be positive or negative.
  EXPECT_EQ(static_cast<int>(sizeof(uint32_t) * width),
            abs((*frame)->stride()));
}

TEST_F(ScreenCapturerMacTest, Capture) {
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .Times(2)
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback1))
      .WillOnce(Invoke(this, &ScreenCapturerMacTest::CaptureDoneCallback2));

  SCOPED_TRACE("");
  capturer_->Start(&callback_);

  // Check that we get an initial full-screen updated.
  capturer_->CaptureFrame();

  // Check that subsequent dirty rects are propagated correctly.
  capturer_->CaptureFrame();
}

std::vector<uint8_t> GetIccData(CFStringRef color_space_name) {
  std::vector<uint8_t> icc_data;
  CGColorSpaceRef color_space = CGColorSpaceCreateWithName(color_space_name);
  if (!color_space)
    return icc_data;

#if !defined(MAC_OS_X_VERSION_10_13) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_13
  rtc::ScopedCFTypeRef<CFDataRef> icc_profile(
      CGColorSpaceCopyICCProfile(color_space));
#else
  rtc::ScopedCFTypeRef<CFDataRef> icc_profile(
      CGColorSpaceCopyICCData(color_space));
#endif
  if (!icc_profile)
    return icc_data;

  const uint8_t* data =
      reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(icc_profile.get()));
  const size_t size = CFDataGetLength(icc_profile.get());
  icc_data = std::vector<uint8_t>(data, data + size);
  return icc_data;
}

TEST(ScreenCapturerMacStandaloneTest, ColorSpace) {
  std::vector<uint8_t> srgb_icc_data = GetIccData(kCGColorSpaceSRGB);
  ASSERT_THAT(srgb_icc_data, Not(IsEmpty()));
  for (const bool allow_iosurface : {false, true}) {
    auto options = DesktopCaptureOptions::CreateDefault();
    options.set_allow_iosurface(allow_iosurface);

    auto capturer = DesktopCapturer::CreateScreenCapturer(std::move(options));

    MockDesktopCapturerCallback callback;
    EXPECT_CALL(callback,
                OnCaptureResultPtr(
                    DesktopCapturer::Result::SUCCESS,
                    ResultOf([](auto frame) { return (*frame)->icc_profile(); },
                             srgb_icc_data)));

    capturer->Start(&callback);

    // Check that the ICC profile is sRGB.
    capturer->CaptureFrame();
  }
}

}  // namespace webrtc
