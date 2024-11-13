/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_DTLS_TRANSPORT_INTERFACE_H_
#define API_DTLS_TRANSPORT_INTERFACE_H_

#include <memory>
#include <optional>
#include <utility>

#include "absl/base/attributes.h"
#include "api/ice_transport_interface.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// States of a DTLS transport, corresponding to the JS API specification.
// http://w3c.github.io/webrtc-pc/#dom-rtcdtlstransportstate
enum class DtlsTransportState {
  kNew,         // Has not started negotiating yet.
  kConnecting,  // In the process of negotiating a secure connection.
  kConnected,   // Completed negotiation and verified fingerprints.
  kClosed,      // Intentionally closed.
  kFailed,      // Failure due to an error or failing to verify a remote
                // fingerprint.
  kNumValues
};

enum class DtlsTransportTlsRole {
  kServer,  // Other end sends CLIENT_HELLO
  kClient   // This end sends CLIENT_HELLO
};

// This object gives snapshot information about the changeable state of a
// DTLSTransport.
class RTC_EXPORT DtlsTransportInformation {
 public:
  DtlsTransportInformation();
  explicit DtlsTransportInformation(DtlsTransportState state);
  DtlsTransportInformation(
      DtlsTransportState state,
      std::optional<DtlsTransportTlsRole> role,
      std::optional<int> tls_version,
      std::optional<int> ssl_cipher_suite,
      std::optional<int> srtp_cipher_suite,
      std::unique_ptr<rtc::SSLCertChain> remote_ssl_certificates);
  ABSL_DEPRECATED("Use version with role parameter")
  DtlsTransportInformation(
      DtlsTransportState state,
      std::optional<int> tls_version,
      std::optional<int> ssl_cipher_suite,
      std::optional<int> srtp_cipher_suite,
      std::unique_ptr<rtc::SSLCertChain> remote_ssl_certificates);

  // Copy and assign
  DtlsTransportInformation(const DtlsTransportInformation& c);
  DtlsTransportInformation& operator=(const DtlsTransportInformation& c);

  // Move
  DtlsTransportInformation(DtlsTransportInformation&& other) = default;
  DtlsTransportInformation& operator=(DtlsTransportInformation&& other) =
      default;

  DtlsTransportState state() const { return state_; }
  void set_state(DtlsTransportState state);

  std::optional<DtlsTransportTlsRole> role() const { return role_; }
  void set_role(DtlsTransportTlsRole role) { role_ = role; }
  std::optional<int> tls_version() const { return tls_version_; }
  void set_tls_version(std::optional<int> version) { tls_version_ = version; }
  std::optional<int> ssl_cipher_suite() const { return ssl_cipher_suite_; }
  void set_ssl_cipher_suite(std::optional<int> suite) {
    ssl_cipher_suite_ = suite;
  }
  std::optional<int> srtp_cipher_suite() const { return srtp_cipher_suite_; }
  void set_srtp_cipher_suite(std::optional<int> suite) {
    srtp_cipher_suite_ = suite;
  }

  // The accessor returns a temporary pointer, it does not release ownership.
  const rtc::SSLCertChain* remote_ssl_certificates() const {
    return remote_ssl_certificates_.get();
  }

  void set_remote_ssl_certificates(
      std::unique_ptr<rtc::SSLCertChain> remote_ssl_certificates) {
    remote_ssl_certificates_ = std::move(remote_ssl_certificates);
  }

 private:
  DtlsTransportState state_;
  std::optional<DtlsTransportTlsRole> role_;
  std::optional<int> tls_version_;
  std::optional<int> ssl_cipher_suite_;
  std::optional<int> srtp_cipher_suite_;
  std::unique_ptr<rtc::SSLCertChain> remote_ssl_certificates_;
};

class DtlsTransportObserverInterface {
 public:
  // This callback carries information about the state of the transport.
  // The argument is a pass-by-value snapshot of the state.
  virtual void OnStateChange(DtlsTransportInformation info) = 0;
  // This callback is called when an error occurs, causing the transport
  // to go to the kFailed state.
  virtual void OnError(RTCError error) = 0;

 protected:
  virtual ~DtlsTransportObserverInterface() = default;
};

// A DTLS transport, as represented to the outside world.
// This object is created on the network thread, and can only be
// accessed on that thread, except for functions explicitly marked otherwise.
// References can be held by other threads, and destruction can therefore
// be initiated by other threads.
class DtlsTransportInterface : public webrtc::RefCountInterface {
 public:
  // Returns a pointer to the ICE transport that is owned by the DTLS transport.
  virtual rtc::scoped_refptr<IceTransportInterface> ice_transport() = 0;
  // Returns information on the state of the DtlsTransport.
  // This function can be called from other threads.
  // TODO(tommi): Stop using (data_channel_integrationtest.cc), deprecate and
  // then remove this accessor. It forces an implementation to add
  // synchronization (e.g. locking) just for the purposes of being able to
  // implement this getter. Instead, a client can rely on the OnStateChange()
  // callbacks and avoid polling.
  virtual DtlsTransportInformation Information() = 0;
  // Observer management.
  virtual void RegisterObserver(DtlsTransportObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;
};

}  // namespace webrtc

#endif  // API_DTLS_TRANSPORT_INTERFACE_H_
