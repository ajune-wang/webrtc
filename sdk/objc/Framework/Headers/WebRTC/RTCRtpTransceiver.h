/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCRtpSender.h>
#import <WebRTC/RTCRtpReceiver.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, RTCRtpTransceiverDirection) {
  RTCRtpTransceiverDirectionSendRecv,
  RTCRtpTransceiverDirectionSendOnly,
  RTCRtpTransceiverDirectionRecvOnly,
  RTCRtpTransceiverDirectionInactive,
};

@interface RTCRtpTransceiverInit : NSObject

@property(nonatomic) RTCRtpTransceiverDirection direction;
@property(nonatomic) NSArray<NSString *> *streamLabels;
@property(nonatomic) NSArray<RTCRtpEncodingParameters *> *sendEncodings;

@end

@class RTCRtpTransceiver;

RTC_EXPORT
@protocol RTCRtpTransceiver <NSObject>

@property(nonatomic, readonly) RTCRtpMediaType mediaType;

@property(nonatomic, readonly) NSString *mid;

@property(nonatomic, readonly) RTCRtpSender *sender;
@property(nonatomic, readonly) RTCRtpReceiver *receiver;

@property(nonatomic, readonly) BOOL isStopped;

@property(nonatomic, readonly) RTCRtpTransceiverDirection direction;

@property(nonatomic, readonly) RTCRtpTransceiverDirection *currentDirection;


@end

RTC_EXPORT
@interface RTCRtpTransceiver : NSObject <RTCRtpTransceiver>

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
