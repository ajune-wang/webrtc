/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/opensslutility.h"

#include <memory>

#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif

#if defined(WEBRTC_WIN)
// Must be included first before openssl headers.
#include "rtc_base/win32.h"  // NOLINT
#endif                       // WEBRTC_WIN

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/openssl.h"
#include "rtc_base/opensslcertificate.h"

namespace rtc {
namespace openssl {

// Holds various helper methods.
namespace {
void LogCertificates(SSL* ssl, X509* certificate) {
// Logging certificates is extremely verbose. So it is disabled by default.
#ifdef LOG_CERTIFICATES
  BIO* mem = BIO_new(BIO_s_mem());
  if (mem == nullptr) {
    RTC_DLOG(LS_ERROR) << "BIO_new() failed to allocate memory.";
    return;
  }

  RTC_DLOG(LS_INFO) << "Certificate from server:";
  X509_print_ex(mem, certificate, XN_FLAG_SEP_CPLUS_SPC, X509_FLAG_NO_HEADER);
  BIO_write(mem, "\0", 1);

  char* buffer = nullptr;
  BIO_get_mem_data(mem, &buffer);
  if (buffer != nullptr) {
    RTC_DLOG(LS_INFO) << buffer;
  } else {
    RTC_DLOG(LS_ERROR) << "BIO_get_mem_data() failed to get buffer.";
  }
  BIO_free(mem);

  const char* cipher_name = SSL_CIPHER_get_name(SSL_get_current_cipher(ssl));
  if (cipher_name != nullptr) {
    RTC_DLOG(LS_INFO) << "Cipher: " << cipher_name;
  } else {
    RTC_DLOG(LS_ERROR) << "SSL_CIPHER_DESCRIPTION() failed to get cipher_name.";
  }
#endif
}
}  // namespace

bool VerifyPeerCertMatchesHost(SSL* ssl, const std::string& host) {
  if (host.empty()) {
    RTC_DLOG(LS_ERROR) << "Hostname is empty. Cannot verify peer certificate.";
    return false;
  }

  if (ssl == nullptr) {
    RTC_DLOG(LS_ERROR) << "SSL is nullptr. Cannot verify peer certificate.";
    return false;
  }

  X509* certificate = SSL_get_peer_certificate(ssl);
  if (certificate == nullptr) {
    RTC_DLOG(LS_ERROR)
        << "SSL_get_peer_certificate failed. This should never happen.";
    return false;
  }

  LogCertificates(ssl, certificate);

  bool is_valid_cert_name =
      X509_check_host(certificate, host.c_str(), host.size(), 0, nullptr) == 1;
  X509_free(certificate);
  return is_valid_cert_name;
}

void LogSSLErrors(const std::string& prefix) {
  char error_buf[200];
  unsigned long err;  // NOLINT

  while ((err = ERR_get_error()) != 0) {
    ERR_error_string_n(err, error_buf, sizeof(error_buf));
    RTC_LOG(LS_ERROR) << prefix << ": " << error_buf << "\n";
  }
}

bool LoadSSLRootCertsIntoTrustStore(SSL_CTX* ssl_ctx,
                                    SSLRootCertLoader* loader) {
  // Validate we have some way to load the root certificates.
  if (loader == nullptr) {
    RTC_LOG(LS_WARNING) << "SSL_CTX creation failed: No SSLRootCertLoader";
    return false;
  }

  // Load the root certificates using the provided certificate loader.
  const SSLCertChain root_certificates(loader->Load());
  size_t num_root_certs = root_certificates.GetSize();

  if (num_root_certs == 0) {
    RTC_LOG(LS_WARNING) << "SSL_CTX creation failed: No certificates loaded";
    return false;
  }

  // Iterate over the root certificates and add them to the OpenSSL trust store.
  size_t valid_certificates_added = 0;
  for (size_t i = 0; i < num_root_certs; i++) {
    // Find a generic high performant way to convert this.
    std::unique_ptr<OpenSSLCertificate> root_cert(
        OpenSSLCertificate::FromPEMString(
            root_certificates.Get(i).ToPEMString()));
    if (X509_STORE_add_cert(SSL_CTX_get_cert_store(ssl_ctx),
                            root_cert->x509()) == 0) {
      RTC_LOG(LS_WARNING) << "Unable to add root certificate.";
    } else {
      valid_certificates_added++;
    }
  }

  // Validate we added at least one certificate.
  if (valid_certificates_added == 0) {
    RTC_LOG(LS_WARNING)
        << "SSL_CTX creation failed: No valid root certificates";
    return false;
  }

  return true;
}

}  // namespace openssl
}  // namespace rtc
