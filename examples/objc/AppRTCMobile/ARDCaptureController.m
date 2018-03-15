/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDCaptureController.h"

#import "ARDSettingsModel.h"
#import "WebRTC/RTCLogging.h"

@implementation ARDCaptureController {
  ARDARVideoCapturer *_capturer;
  ARDSettingsModel *_settings;
}

- (instancetype)initWithCapturer:(ARDARVideoCapturer *)capturer
                        settings:(ARDSettingsModel *)settings {
  if (self = [super init]) {
    _capturer = capturer;
    _settings = settings;
  }

  return self;
}

- (void)startCapture {
  [_capturer startCapture];
}

- (void)stopCapture {
  [_capturer stopCapture];
}
@end
