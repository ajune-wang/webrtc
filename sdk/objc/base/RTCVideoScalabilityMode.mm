/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoScalabilityMode.h"
#import "RTCMacros.h"

#include "absl/types/optional.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/scalability_mode_helper.h"

@implementation RTC_OBJC_TYPE (RTCVideoScalabilityMode)

webrtc::ScalabilityMode _value;

- (instancetype)initWithValue:(webrtc::ScalabilityMode)value {
  if (self = [super init]) {
    _value = value;
  }
  return self;
}

- (instancetype)initWithName:(nonnull NSString *)name {
  if (name == nil) {
    return nil;
  }
  absl::optional<webrtc::ScalabilityMode> mode =
      webrtc::ScalabilityModeStringToEnum([name UTF8String]);
  if (!mode.has_value()) {
    return nil;
  }
  return [self initWithValue:*mode];
}

- (webrtc::ScalabilityMode)value {
  return _value;
}

- (nonnull NSString *)name {
  absl::string_view nativeName = webrtc::ScalabilityModeToString(_value);
  return [[NSString alloc] initWithBytes:nativeName.data()
                                  length:nativeName.size()
                                encoding:NSUTF8StringEncoding];
}

@end
