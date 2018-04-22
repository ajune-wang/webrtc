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
#include "rtc_base/arraysize.h"
#endif

#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"

NSString *RTCMaxSupportedProfileLevelConstrainedHigh();
NSString *RTCMaxSupportedProfileLevelConstrainedBaseline();

NSString *const kRTCVideoCodecVp8Name = @(cricket::kVp8CodecName);
NSString *const kRTCVideoCodecVp9Name = @(cricket::kVp9CodecName);
NSString *const kRTCVideoCodecH264Name = @(cricket::kH264CodecName);

NSString *const kRTCMaxSupportedConstrainedHigh = RTCMaxSupportedProfileLevelConstrainedHigh();

NSString *const kRTCMaxSupportedConstrainedBaseline =
    RTCMaxSupportedProfileLevelConstrainedBaseline();

namespace {

NSString *const kRTCLevel31ConstrainedHigh = @"640c1f";
NSString *const kRTCLevel31ConstrainedBaseline = @"42e01f";

#if defined(WEBRTC_IOS)

using namespace webrtc::H264;

struct {
  const RTCDeviceType deviceType;
  const ProfileLevelId profile;
} const kH264MaxSupportedProfiles[] = {
    // iPhones with at least iOS 9
    {RTCDeviceTypeIPhoneX, {kProfileHigh, kLevel5_2}},       // https://support.apple.com/kb/SP770
    {RTCDeviceTypeIPhone8, {kProfileHigh, kLevel5_2}},       // https://support.apple.com/kb/SP767
    {RTCDeviceTypeIPhone8Plus, {kProfileHigh, kLevel5_2}},   // https://support.apple.com/kb/SP768
    {RTCDeviceTypeIPhone7, {kProfileHigh, kLevel5_1}},       // https://support.apple.com/kb/SP743
    {RTCDeviceTypeIPhone7Plus, {kProfileHigh, kLevel5_1}},   // https://support.apple.com/kb/SP744
    {RTCDeviceTypeIPhoneSE, {kProfileHigh, kLevel4_2}},      // https://support.apple.com/kb/SP738
    {RTCDeviceTypeIPhone6S, {kProfileHigh, kLevel4_2}},      // https://support.apple.com/kb/SP726
    {RTCDeviceTypeIPhone6SPlus, {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP727
    {RTCDeviceTypeIPhone6, {kProfileHigh, kLevel4_2}},       // https://support.apple.com/kb/SP705
    {RTCDeviceTypeIPhone6Plus, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/SP706
    {RTCDeviceTypeIPhone5SGSM, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/SP685
    {RTCDeviceTypeIPhone5SGSM_CDMA,
     {kProfileHigh, kLevel4_2}},                           // https://support.apple.com/kb/SP685
    {RTCDeviceTypeIPhone5GSM, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP655
    {RTCDeviceTypeIPhone5GSM_CDMA,
     {kProfileHigh, kLevel4_1}},                            // https://support.apple.com/kb/SP655
    {RTCDeviceTypeIPhone5CGSM, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP684
    {RTCDeviceTypeIPhone5CGSM_CDMA,
     {kProfileHigh, kLevel4_1}},                         // https://support.apple.com/kb/SP684
    {RTCDeviceTypeIPhone4S, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP643

    // iPods with at least iOS 9
    {RTCDeviceTypeIPodTouch6G, {kProfileMain, kLevel4_1}},  // https://support.apple.com/kb/SP720
    {RTCDeviceTypeIPodTouch5G, {kProfileMain, kLevel3_1}},  // https://support.apple.com/kb/SP657

    // iPads with at least iOS 9
    {RTCDeviceTypeIPad2Wifi, {kProfileHigh, kLevel4_1}},     // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2GSM, {kProfileHigh, kLevel4_1}},      // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2CDMA, {kProfileHigh, kLevel4_1}},     // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2Wifi2, {kProfileHigh, kLevel4_1}},    // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPadMiniWifi, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPadMiniGSM, {kProfileHigh, kLevel4_1}},   // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPadMiniGSM_CDMA,
     {kProfileHigh, kLevel4_1}},                              // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPad3Wifi, {kProfileHigh, kLevel4_1}},      // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad3GSM_CDMA, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad3GSM, {kProfileHigh, kLevel4_1}},       // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad4Wifi, {kProfileHigh, kLevel4_1}},      // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad4GSM, {kProfileHigh, kLevel4_1}},       // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad4GSM_CDMA, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad5, {kProfileHigh, kLevel4_2}},          // https://support.apple.com/kb/SP751
    {RTCDeviceTypeIPad6, {kProfileHigh, kLevel4_2}},          // https://support.apple.com/kb/SP774
    {RTCDeviceTypeIPadAirWifi, {kProfileHigh, kLevel4_2}},    // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAirCellular,
     {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAirWifiCellular,
     {kProfileHigh, kLevel4_2}},                               // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAir2, {kProfileHigh, kLevel4_2}},        // https://support.apple.com/kb/SP708
    {RTCDeviceTypeIPadMini2GWifi, {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini2GCellular,
     {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini2GWifiCellular,
     {kProfileHigh, kLevel4_2}},                               // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini3, {kProfileHigh, kLevel4_2}},       // https://support.apple.com/kb/SP709
    {RTCDeviceTypeIPadMini4, {kProfileHigh, kLevel4_2}},       // https://support.apple.com/kb/SP725
    {RTCDeviceTypeIPadPro9Inch, {kProfileHigh, kLevel4_2}},    // https://support.apple.com/kb/SP739
    {RTCDeviceTypeIPadPro12Inch, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/sp723
    {RTCDeviceTypeIPadPro12Inch2, {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP761
    {RTCDeviceTypeIPadPro10Inch, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/SP762
};

rtc::Optional<ProfileLevelId> FindMaxSupportedProfileForDevice(RTCDeviceType deviceType) {
  for (size_t i = 0; i < arraysize(kH264MaxSupportedProfiles); ++i) {
    const auto &supportedProfile = kH264MaxSupportedProfiles[i];
    if (supportedProfile.deviceType == deviceType) {
      return supportedProfile.profile;
    }
  }
  return rtc::nullopt;
}

NSString *RTCMaxSupportedLevelForProfile(Profile profile) {
  const auto &profileLevelId = FindMaxSupportedProfileForDevice([UIDevice deviceType]);
  if (profileLevelId && profileLevelId->profile >= profile) {
    const auto &profileString =
        ProfileLevelIdToString(ProfileLevelId(profile, profileLevelId->level));
    if (profileString) {
      return [NSString stringForStdString:*profileString];
    }
  }
  return nil;
}

#endif

}  // namespace

NSString *RTCMaxSupportedProfileLevelConstrainedBaseline() {
#if defined(WEBRTC_IOS)
  NSString *profile = RTCMaxSupportedLevelForProfile(webrtc::H264::kProfileConstrainedBaseline);
  if (profile != nil) {
    return profile;
  }
#endif
  return kRTCLevel31ConstrainedBaseline;
}

NSString *RTCMaxSupportedProfileLevelConstrainedHigh() {
#if defined(WEBRTC_IOS)
  NSString *profile = RTCMaxSupportedLevelForProfile(webrtc::H264::kProfileConstrainedHigh);
  if (profile != nil) {
    return profile;
  }
#endif
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
