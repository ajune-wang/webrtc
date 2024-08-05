/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

#include "api/video_codecs/scalability_mode.h"

/** Represents scalability mode for video encoding. Corresponds to webrtc::ScalabilityMode enum. */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCVideoScalabilityMode) : NSObject

- (nullable instancetype)init NS_UNAVAILABLE;
- (nonnull instancetype)initWithValue:(webrtc::ScalabilityMode)value;
- (nullable instancetype)initWithName:(nonnull NSString *)name;

- (webrtc::ScalabilityMode)value;
- (nonnull NSString *)name;

@end
