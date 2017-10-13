/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCVideoCodec.h>

NS_ASSUME_NONNULL_BEGIN

/** RTCVideoEncoderFactory is an Objective-C version of webrtc::VideoEncoderFactory. */
RTC_EXPORT
@protocol RTCVideoEncoderFactory <NSObject>

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info;
- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@end

/** RTCVideoDecoderFactory is an Objective-C version of webrtc::VideoDecoderFactory. */
RTC_EXPORT
@protocol RTCVideoDecoderFactory <NSObject>

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info;
- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@end

#pragma mark - Codec List for Default Factories

/* This class holds a dictionary of codec types to encoder/decoder classes and keeps track of
 * the order in which the classes were added, in order to prioritize them in the factory.
 * The codecs are added in order of priority, with the first one having highest priority.
 */
RTC_EXPORT
@interface RTCVideoCodecPriorityList <ObjectType> : NSObject

/* Add a supported codec type, and a backing RTCVideoEncoder/Decoder class. */
- (void)addCodecInfo:(RTCVideoCodecInfo *)codecInfo withClass:(ObjectType)codecClass;

/* Returns the codec types in the order they were added to the list. */
- (NSArray<RTCVideoCodecInfo *> *)codecsByPriority;

/* Returns the encoder/decoder class associated with the given codec type. */
- (ObjectType)codecClassForCodecInfo:(RTCVideoCodecInfo *)codecInfo;

@end

#pragma mark - Default Factories

RTC_EXPORT
@interface RTCDefaultVideoEncoderFactory : NSObject <RTCVideoEncoderFactory>

- (instancetype)initWithEncoderPriorityList:
        (RTCVideoCodecPriorityList<Class<RTCVideoEncoder>> *)codecList;

@end

RTC_EXPORT
@interface RTCDefaultVideoDecoderFactory : NSObject <RTCVideoDecoderFactory>

- (instancetype)initWithDecoderPriorityList:
        (RTCVideoCodecPriorityList<Class<RTCVideoDecoder>> *)codecList;

@end

NS_ASSUME_NONNULL_END
