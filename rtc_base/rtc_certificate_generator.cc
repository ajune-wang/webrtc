/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rtc_certificate_generator.h"

#include <time.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/futures/interop.h"
#include "rtc_base/location.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/message_queue.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/ssl_identity.h"

namespace rtc {

namespace {

// A certificates' subject and issuer name.
const char kIdentityName[] = "WebRTC";
const uint64_t kYearInSeconds = 365 * 24 * 60 * 60;

}  // namespace

// static
scoped_refptr<RTCCertificate> RTCCertificateGenerator::GenerateCertificate(
    const KeyParams& key_params,
    const absl::optional<uint64_t>& expires_ms) {
  if (!key_params.IsValid()) {
    return nullptr;
  }

  SSLIdentity* identity = nullptr;
  if (!expires_ms) {
    identity = SSLIdentity::Generate(kIdentityName, key_params);
  } else {
    uint64_t expires_s = *expires_ms / 1000;
    // Limit the expiration time to something reasonable (a year). This was
    // somewhat arbitrarily chosen. It also ensures that the value is not too
    // large for the unspecified |time_t|.
    expires_s = std::min(expires_s, kYearInSeconds);
    // TODO(torbjorng): Stop using |time_t|, its type is unspecified. It it safe
    // to assume it can hold up to a year's worth of seconds (and more), but
    // |SSLIdentity::Generate| should stop relying on |time_t|.
    // See bugs.webrtc.org/5720.
    time_t cert_lifetime_s = static_cast<time_t>(expires_s);
    identity = SSLIdentity::GenerateWithExpiration(kIdentityName, key_params,
                                                   cert_lifetime_s);
  }
  if (!identity) {
    return nullptr;
  }
  std::unique_ptr<SSLIdentity> identity_sptr(identity);
  return RTCCertificate::Create(std::move(identity_sptr));
}

RTCCertificateGenerator::RTCCertificateGenerator(Thread* signaling_thread,
                                                 Thread* worker_thread)
    : signaling_thread_(signaling_thread), worker_thread_(worker_thread) {
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);
}

std::unique_ptr<webrtc::Future<scoped_refptr<RTCCertificate>>>
RTCCertificateGenerator::GenerateCertificateAsync(
    const KeyParams& key_params,
    const absl::optional<uint64_t>& expires_ms) {
  auto generate_fn = [=]() {
    return RTCCertificateGenerator::GenerateCertificate(key_params, expires_ms);
  };
  return std::make_unique<
      webrtc::AsyncCallbackFuture<scoped_refptr<RTCCertificate>>>(
      [worker_thread = worker_thread_, generate_fn = std::move(generate_fn)](
          std::function<void(scoped_refptr<RTCCertificate>)> complete_cb) {
        auto worker_fn = [generate_fn = std::move(generate_fn),
                          complete_cb = std::move(complete_cb)]() {
          complete_cb(generate_fn());
        };
        worker_thread->PostTask(RTC_FROM_HERE, std::move(worker_fn));
      });
}

}  // namespace rtc
