/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#import "NSString+StdString.h"
#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodecFactory.h"

#if defined(WEBRTC_IOS)
#import "WebRTC/UIDevice+RTCDevice.h"
#endif

#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"

NSString *RTCMaxSupportedProfileLevelConstrainedHigh();
NSString *RTCMaxSupportedProfileLevelConstrainedBaseline();

NSString *const kRTCVideoCodecVp8Name = @(cricket::kVp8CodecName);
NSString *const kRTCVideoCodecVp9Name = @(cricket::kVp9CodecName);
NSString *const kRTCVideoCodecH264Name = @(cricket::kH264CodecName);

NSString *const kRTCLevel31ConstrainedHigh = @"640c1f";
NSString *const kRTCLevel31ConstrainedBaseline = @"42e01f";
NSString *const kRTCMaxSupportedConstrainedHigh = RTCMaxSupportedProfileLevelConstrainedHigh();
NSString *const kRTCMaxSupportedConstrainedBaseline =
    RTCMaxSupportedProfileLevelConstrainedBaseline();

NSString *RTCMaxSupportedProfileLevelConstrainedBaseline() {
  rtc::Optional<webrtc::H264::Level> level;
#if defined(WEBRTC_IOS)
  switch ([UIDevice deviceType]) {
    case RTCDeviceTypeIPhoneX:
    case RTCDeviceTypeIPhone8Plus:
    case RTCDeviceTypeIPhone8:
      // iPhone 8 spec https://support.apple.com/kb/SP767
      // iPhone 8+ spec https://support.apple.com/kb/SP768
      // iPhone X https://support.apple.com/kb/SP770
      level = webrtc::H264::kLevel5_2;
      break;

    case RTCDeviceTypeIPhone7Plus:
    case RTCDeviceTypeIPhone7:
      // iPhone 7 spec https://support.apple.com/kb/SP743
      // iPhone 7+ spec https://support.apple.com/kb/SP744
      level = webrtc::H264::kLevel5_1;
      break;

    case RTCDeviceTypeIPhone5SGSM:
    case RTCDeviceTypeIPhone5SGSM_CDMA:
    case RTCDeviceTypeIPhone6SPlus:
    case RTCDeviceTypeIPhone6S:
    case RTCDeviceTypeIPhone6Plus:
    case RTCDeviceTypeIPhone6:
      // iPhone 5 spec https://support.apple.com/kb/sp685
      // iPhone 6 spec https://support.apple.com/kb/SP705
      // iPhone 6+ spec https://support.apple.com/kb/SP706
      // iPhone 6S spec https://support.apple.com/kb/SP726
      // iPhone 6S+ spec https://support.apple.com/kb/SP727
      level = webrtc::H264::kLevel4_2;
      break;

    case RTCDeviceTypeIPhone5GSM:
    case RTCDeviceTypeIPhone5GSM_CDMA:
    case RTCDeviceTypeIPhone5CGSM:
    case RTCDeviceTypeIPhone5CGSM_CDMA:
    case RTCDeviceTypeIPhone4S:
      // iPhone 4S spec https://support.apple.com/kb/SP643
      // iPhone 5 spec https://support.apple.com/kb/SP655
      // iPhone 5c spec https://support.apple.com/kb/sp684
      level = webrtc::H264::kLevel4_1;
      break;

    case RTCDeviceTypeIPhone4Verizon:
    case RTCDeviceTypeIPhone4:
      // iPhone 4 spec https://support.apple.com/kb/SP587
      level = webrtc::H264::kLevel3_1;
      break;

    case RTCDeviceTypeIPhone3GS:
      // iPhone 3GS spec https://support.apple.com/kb/SP565
      level = webrtc::H264::kLevel3;
      break;

    case RTCDeviceTypeIPhone3G:
    case RTCDeviceTypeIPhone1G:
      RTC_NOTREACHED();
      break;

    case RTCDeviceTypeIPodTouch1G:
    case RTCDeviceTypeIPodTouch2G:
    case RTCDeviceTypeIPodTouch3G:
      // iPod touch 1st gen https://support.apple.com/kb/SP3
      // iPod touch 2nd gen https://support.apple.com/kb/SP496
      // iPod touch 3rd gen https://support.apple.com/kb/SP570
      level = webrtc::H264::kLevel3;
      break;

    case RTCDeviceTypeIPodTouch4G:
    case RTCDeviceTypeIPodTouch5G:
      // iPod touch 4th gen https://support.apple.com/kb/sp594
      // iPod touch 5th gen https://support.apple.com/kb/sp657
      level = webrtc::H264::kLevel3_1;
      break;

    case RTCDeviceTypeIPodTouch6G:
      // iPod touch 6th gen https://support.apple.com/kb/SP720
      level = webrtc::H264::kLevel4_1;
      break;

    case RTCDeviceTypeIPad:
      // iPad spec https://support.apple.com/kb/sp580
      level = webrtc::H264::kLevel3_1;
      break;

    case RTCDeviceTypeIPad2Wifi:
    case RTCDeviceTypeIPad2GSM:
    case RTCDeviceTypeIPad2CDMA:
    case RTCDeviceTypeIPad2Wifi2:
    case RTCDeviceTypeIPadMiniWifi:
    case RTCDeviceTypeIPadMiniGSM:
    case RTCDeviceTypeIPadMiniGSM_CDMA:
    case RTCDeviceTypeIPad3Wifi:
    case RTCDeviceTypeIPad3GSM_CDMA:
    case RTCDeviceTypeIPad3GSM:
    case RTCDeviceTypeIPad4Wifi:
    case RTCDeviceTypeIPad4GSM:
    case RTCDeviceTypeIPad4GSM_CDMA:
      // iPad 2 spec https://support.apple.com/kb/sp622
      // iPad mini spec https://support.apple.com/kb/sp661
      // iPad 3 spec https://support.apple.com/kb/sp647
      // iPad 4 spec https://support.apple.com/kb/sp662
      level = webrtc::H264::kLevel4_1;
      break;

    case RTCDeviceTypeIPadMini2GWifi:
    case RTCDeviceTypeIPadMini2GCellular:
    case RTCDeviceTypeIPadAirWifi:
    case RTCDeviceTypeIPadAirCellular:
      // iPad Air https://support.apple.com/kb/sp692
      // iPad mini 2 https://support.apple.com/kb/sp693
      level = webrtc::H264::kLevel4_2;
      break;

    case RTCDeviceTypeSimulatori386:
    case RTCDeviceTypeSimulatorx86_64:
    case RTCDeviceTypeUnknown:
    default:
      break;
  }
#endif
  if (level) {
    auto profileString = webrtc::H264::ProfileLevelIdToString(
        webrtc::H264::ProfileLevelId(webrtc::H264::kProfileConstrainedBaseline, *level));
    if (profileString) {
      return [NSString stringForStdString:*profileString];
    }
  }

  return kRTCLevel31ConstrainedBaseline;
}

NSString *RTCMaxSupportedProfileLevelConstrainedHigh() {
  rtc::Optional<webrtc::H264::Level> level;
#if defined(WEBRTC_IOS)
  switch ([UIDevice deviceType]) {
    case RTCDeviceTypeIPhoneX:
    case RTCDeviceTypeIPhone8Plus:
    case RTCDeviceTypeIPhone8:
      // iPhone 8 spec https://support.apple.com/kb/SP767
      // iPhone 8+ spec https://support.apple.com/kb/SP768
      // iPhone X https://support.apple.com/kb/SP770
      level = webrtc::H264::kLevel5_2;
      break;

    case RTCDeviceTypeIPhone7Plus:
    case RTCDeviceTypeIPhone7:
      // iPhone 7 spec https://support.apple.com/kb/SP743
      // iPhone 7+ spec https://support.apple.com/kb/SP744
      level = webrtc::H264::kLevel5_1;
      break;

    case RTCDeviceTypeIPhone5SGSM:
    case RTCDeviceTypeIPhone5SGSM_CDMA:
    case RTCDeviceTypeIPhone6SPlus:
    case RTCDeviceTypeIPhone6S:
    case RTCDeviceTypeIPhone6Plus:
    case RTCDeviceTypeIPhone6:
      // iPhone 5 spec https://support.apple.com/kb/sp685
      // iPhone 6 spec https://support.apple.com/kb/SP705
      // iPhone 6+ spec https://support.apple.com/kb/SP706
      // iPhone 6S spec https://support.apple.com/kb/SP726
      // iPhone 6S+ spec https://support.apple.com/kb/SP727
      level = webrtc::H264::kLevel4_2;
      break;

    case RTCDeviceTypeIPhone5GSM:
    case RTCDeviceTypeIPhone5GSM_CDMA:
    case RTCDeviceTypeIPhone5CGSM:
    case RTCDeviceTypeIPhone5CGSM_CDMA:
    case RTCDeviceTypeIPhone4S:
      // iPhone 4S spec https://support.apple.com/kb/SP643
      // iPhone 5 spec https://support.apple.com/kb/SP655
      // iPhone 5c spec https://support.apple.com/kb/sp684
      level = webrtc::H264::kLevel4_1;
      break;

    case RTCDeviceTypeIPhone4Verizon:
    case RTCDeviceTypeIPhone4:
      // iPhone 4 spec https://support.apple.com/kb/SP587
      level = rtc::nullopt;
      break;

    case RTCDeviceTypeIPhone3GS:
      // iPhone 3GS spec https://support.apple.com/kb/SP565
      level = rtc::nullopt;
      break;

    case RTCDeviceTypeIPhone3G:
    case RTCDeviceTypeIPhone1G:
      RTC_NOTREACHED();
      break;

    case RTCDeviceTypeIPodTouch1G:
    case RTCDeviceTypeIPodTouch2G:
    case RTCDeviceTypeIPodTouch3G:
      // iPod touch 1st gen https://support.apple.com/kb/SP3
      // iPod touch 2nd gen https://support.apple.com/kb/SP496
      // iPod touch 3rd gen https://support.apple.com/kb/SP570
      level = rtc::nullopt;
      break;

    case RTCDeviceTypeIPodTouch4G:
    case RTCDeviceTypeIPodTouch5G:
      // iPod touch 4th gen https://support.apple.com/kb/sp594
      // iPod touch 5th gen https://support.apple.com/kb/sp657
      level = rtc::nullopt;
      break;

    case RTCDeviceTypeIPodTouch6G:
      // iPod touch 6th gen https://support.apple.com/kb/SP720
      level = rtc::nullopt;
      break;

    case RTCDeviceTypeIPad:
      // iPad spec https://support.apple.com/kb/sp580
      level = rtc::nullopt;
      break;

    case RTCDeviceTypeIPad2Wifi:
    case RTCDeviceTypeIPad2GSM:
    case RTCDeviceTypeIPad2CDMA:
    case RTCDeviceTypeIPad2Wifi2:
    case RTCDeviceTypeIPadMiniWifi:
    case RTCDeviceTypeIPadMiniGSM:
    case RTCDeviceTypeIPadMiniGSM_CDMA:
    case RTCDeviceTypeIPad3Wifi:
    case RTCDeviceTypeIPad3GSM_CDMA:
    case RTCDeviceTypeIPad3GSM:
    case RTCDeviceTypeIPad4Wifi:
    case RTCDeviceTypeIPad4GSM:
    case RTCDeviceTypeIPad4GSM_CDMA:
      // iPad 2 spec https://support.apple.com/kb/sp622
      // iPad mini spec https://support.apple.com/kb/sp661
      // iPad 3 spec https://support.apple.com/kb/sp647
      // iPad 4 spec https://support.apple.com/kb/sp662
      level = webrtc::H264::kLevel4_1;
      break;

    case RTCDeviceTypeIPadMini2GWifi:
    case RTCDeviceTypeIPadMini2GCellular:
    case RTCDeviceTypeIPadAirWifi:
    case RTCDeviceTypeIPadAirCellular:
      // iPad Air https://support.apple.com/kb/sp692
      // iPad mini 2 https://support.apple.com/kb/sp693
      level = webrtc::H264::kLevel4_2;
      break;

    case RTCDeviceTypeSimulatori386:
    case RTCDeviceTypeSimulatorx86_64:
    case RTCDeviceTypeUnknown:
    default:
      break;
  }
#endif
  if (level) {
    auto profileString = webrtc::H264::ProfileLevelIdToString(
        webrtc::H264::ProfileLevelId(webrtc::H264::kProfileConstrainedHigh, *level));
    if (profileString) {
      return [NSString stringForStdString:*profileString];
    }
  }

  return kRTCLevel31ConstrainedHigh;
}

@implementation RTCVideoCodecInfo

@synthesize name = _name;
@synthesize parameters = _parameters;

- (instancetype)initWithName:(NSString *)name {
  return [self initWithName:name parameters:nil];
}

- (instancetype)initWithName:(NSString *)name
                  parameters:(nullable NSDictionary<NSString *, NSString *> *)parameters {
  if (self = [super init]) {
    _name = name;
    _parameters = (parameters ? parameters : @{});
  }

  return self;
}

- (instancetype)initWithNativeSdpVideoFormat:(webrtc::SdpVideoFormat)format {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = format.parameters.begin(); it != format.parameters.end(); ++it) {
    [params setObject:[NSString stringForStdString:it->second]
               forKey:[NSString stringForStdString:it->first]];
  }
  return [self initWithName:[NSString stringForStdString:format.name] parameters:params];
}

- (instancetype)initWithNativeVideoCodec:(cricket::VideoCodec)videoCodec {
  return [self
      initWithNativeSdpVideoFormat:webrtc::SdpVideoFormat(videoCodec.name, videoCodec.params)];
}

- (BOOL)isEqualToCodecInfo:(RTCVideoCodecInfo *)info {
  if (!info ||
      ![self.name isEqualToString:info.name] ||
      ![self.parameters isEqualToDictionary:info.parameters]) {
    return NO;
  }
  return YES;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;
  if (![object isKindOfClass:[self class]])
    return NO;
  return [self isEqualToCodecInfo:object];
}

- (NSUInteger)hash {
  return [self.name hash] ^ [self.parameters hash];
}

- (webrtc::SdpVideoFormat)nativeSdpVideoFormat {
  std::map<std::string, std::string> parameters;
  for (NSString *paramKey in _parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:_parameters[paramKey]];
    parameters[key] = value;
  }

  return webrtc::SdpVideoFormat([NSString stdStringForString:_name], parameters);
}

- (cricket::VideoCodec)nativeVideoCodec {
  cricket::VideoCodec codec([NSString stdStringForString:_name]);
  for (NSString *paramKey in _parameters.allKeys) {
    codec.SetParam([NSString stdStringForString:paramKey],
                   [NSString stdStringForString:_parameters[paramKey]]);
  }

  return codec;
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder *)decoder {
  return [self initWithName:[decoder decodeObjectForKey:@"name"]
                 parameters:[decoder decodeObjectForKey:@"parameters"]];
}

- (void)encodeWithCoder:(NSCoder *)encoder {
  [encoder encodeObject:_name forKey:@"name"];
  [encoder encodeObject:_parameters forKey:@"parameters"];
}

@end

@implementation RTCVideoEncoderQpThresholds

@synthesize low = _low;
@synthesize high = _high;

- (instancetype)initWithThresholdsLow:(NSInteger)low high:(NSInteger)high {
  if (self = [super init]) {
    _low = low;
    _high = high;
  }
  return self;
}

@end
