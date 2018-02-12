/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TRANSPORTFACTORYINTERFACE_H_
#define P2P_BASE_TRANSPORTFACTORYINTERFACE_H_

#include <memory>
#include <string>

#include "p2p/base/dtlstransportinternal.h"
#include "p2p/base/icetransportinternal.h"

namespace cricket {

// This interface is used to create P2P layer transports and it can be passed in
// the JsepTransportController. If this is set, the JsepTransportController will
// use it to create DtlsTransports and IceTransports.
class TransportFactoryInterface {
 public:
  virtual ~TransportFactoryInterface() {}

  virtual std::unique_ptr<IceTransportInternal> CreateIceTransport(
      const std::string& transport_name,
      int component) = 0;

  virtual std::unique_ptr<DtlsTransportInternal> CreateDtlsTransport(
      std::unique_ptr<IceTransportInternal> ice,
      const rtc::CryptoOptions& crypto_options) = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_TRANSPORTFACTORYINTERFACE_H_
