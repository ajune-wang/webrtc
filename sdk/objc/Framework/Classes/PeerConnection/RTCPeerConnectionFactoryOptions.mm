/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactoryOptions+Private.h"

#include "rtc_base/network_constants.h"

namespace {

void setNetworkBit(webrtc::PeerConnectionFactoryInterface::Options* options,
                   rtc::AdapterType type,
                   bool ignore) {
  if (ignore) {
    options->network_ignore_mask |= type;
  } else {
    options->network_ignore_mask &= ~type;
  }
}

void readNetworkBit(const webrtc::PeerConnectionFactoryInterface::Options& options,
                    rtc::AdapterType type,
                    BOOL* ignore) {
  *ignore = (options.network_ignore_mask & type) ? YES : NO;
}

}  // namespace

@implementation RTCPeerConnectionFactoryOptions

@synthesize disableEncryption = _disableEncryption;
@synthesize disableNetworkMonitor = _disableNetworkMonitor;
@synthesize ignoreLoopbackNetworkAdapter = _ignoreLoopbackNetworkAdapter;
@synthesize ignoreVPNNetworkAdapter = _ignoreVPNNetworkAdapter;
@synthesize ignoreCellularNetworkAdapter = _ignoreCellularNetworkAdapter;
@synthesize ignoreWiFiNetworkAdapter = _ignoreWiFiNetworkAdapter;
@synthesize ignoreEthernetNetworkAdapter = _ignoreEthernetNetworkAdapter;
@synthesize enableAes128Sha1_32CryptoCipher = _enableAes128Sha1_32CryptoCipher;

- (instancetype)init {
  // Copy defaults.
  webrtc::PeerConnectionFactoryInterface::Options options;
  return [self initWithNativeOptions:options];
}

- (instancetype)initWithNativeOptions:
    (const webrtc::PeerConnectionFactoryInterface::Options &)options {
  if (self = [super init]) {
    _disableEncryption = options.disable_encryption;
    _disableNetworkMonitor = options.disable_network_monitor;

    readNetworkBit(options, rtc::ADAPTER_TYPE_LOOPBACK, &_ignoreLoopbackNetworkAdapter);
    readNetworkBit(options, rtc::ADAPTER_TYPE_VPN, &_ignoreVPNNetworkAdapter);
    readNetworkBit(options, rtc::ADAPTER_TYPE_CELLULAR, &_ignoreCellularNetworkAdapter);
    readNetworkBit(options, rtc::ADAPTER_TYPE_WIFI, &_ignoreWiFiNetworkAdapter);
    readNetworkBit(options, rtc::ADAPTER_TYPE_ETHERNET, &_ignoreEthernetNetworkAdapter);

    _enableAes128Sha1_32CryptoCipher = options.crypto_options.enable_aes128_sha1_32_crypto_cipher;
  }
  return self;
}

- (webrtc::PeerConnectionFactoryInterface::Options)nativeOptions {
  webrtc::PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = self.disableEncryption;
  options.disable_network_monitor = self.disableNetworkMonitor;

  setNetworkBit(&options, rtc::ADAPTER_TYPE_LOOPBACK, self.ignoreLoopbackNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_VPN, self.ignoreVPNNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_CELLULAR, self.ignoreCellularNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_WIFI, self.ignoreWiFiNetworkAdapter);
  setNetworkBit(&options, rtc::ADAPTER_TYPE_ETHERNET, self.ignoreEthernetNetworkAdapter);

  options.crypto_options.enable_aes128_sha1_32_crypto_cipher = self.enableAes128Sha1_32CryptoCipher;

  return options;
}

@end
