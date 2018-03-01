/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDAppDelegateNative.h"

#import "ios/ARDMainViewController.h"
#import "native/ARDAppClientNative.h"

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/event_tracer.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssladapter.h"
#include "system_wrappers/include/field_trial_default.h"

static std::unique_ptr<char[]> gFieldTrialInitString;

@implementation ARDAppDelegateNative {
  UIWindow *_window;
}

#pragma mark - UIApplicationDelegate methods

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  // gFieldTrialInitString.reset("WebRTC-H264HighProfile/Enabled/");
  webrtc::field_trial::InitFieldTrialsFromString("WebRTC-H264HighProfile/Enabled/");

  RTC_DCHECK(rtc::InitializeSSL());
  rtc::tracing::SetupInternalTracer();

  _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [_window makeKeyAndVisible];
  ARDMainViewController *viewController =
      [[ARDMainViewController alloc] initWithClientClass:[ARDAppClientNative class]];

  UINavigationController *root =
      [[UINavigationController alloc] initWithRootViewController:viewController];
  root.navigationBar.translucent = NO;
  _window.rootViewController = root;

#if defined(NDEBUG)
  // In debug builds the default level is LS_INFO and in non-debug builds it is
  // disabled. Continue to log to console in non-debug builds, but only
  // warnings and errors.
  rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
#endif

  return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
  rtc::tracing::ShutdownInternalTracer();
  RTC_DCHECK(rtc::CleanupSSL());
}

@end
