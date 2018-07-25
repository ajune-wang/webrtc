/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jsepicecandidate.h"

#include <memory>
#include <vector>

#include "pc/webrtcsdp.h"
#include "rtc_base/stringencode.h"

namespace webrtc {

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error) {
  JsepIceCandidate* jsep_ice = new JsepIceCandidate(sdp_mid, sdp_mline_index);
  if (!jsep_ice->Initialize(sdp, error)) {
    delete jsep_ice;
    return NULL;
  }
  return jsep_ice;
}

std::unique_ptr<IceCandidateInterface> CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const cricket::Candidate& candidate) {
  return absl::make_unique<JsepIceCandidate>(sdp_mid, sdp_mline_index,
                                             candidate);
}

JsepIceCandidate::JsepIceCandidate(const std::string& sdp_mid,
                                   int sdp_mline_index)
    : sdp_mid_(sdp_mid), sdp_mline_index_(sdp_mline_index) {}

JsepIceCandidate::JsepIceCandidate(const std::string& sdp_mid,
                                   int sdp_mline_index,
                                   const cricket::Candidate& candidate)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(sdp_mline_index),
      candidate_(candidate) {}

JsepIceCandidate::~JsepIceCandidate() = default;

bool JsepIceCandidate::Initialize(const std::string& sdp, SdpParseError* err) {
  return SdpDeserializeCandidate(sdp, this, err);
}

std::string JsepIceCandidate::sdp_mid() const {
  return sdp_mid_;
}

int JsepIceCandidate::sdp_mline_index() const {
  return sdp_mline_index_;
}

const cricket::Candidate& JsepIceCandidate::candidate() const {
  return candidate_;
}

std::string JsepIceCandidate::server_url() const {
  return candidate_.url();
}

bool JsepIceCandidate::ToString(std::string* out) const {
  if (!out)
    return false;
  *out = SdpSerializeCandidate(*this);
  return !out->empty();
}

JsepCandidateCollection::JsepCandidateCollection() = default;

JsepCandidateCollection::JsepCandidateCollection(JsepCandidateCollection&& o)
    : candidates_(std::move(o.candidates_)) {}

JsepCandidateCollection::~JsepCandidateCollection() {
  for (std::vector<JsepIceCandidate*>::iterator it = candidates_.begin();
       it != candidates_.end(); ++it) {
    delete *it;
  }
}

size_t JsepCandidateCollection::count() const {
  return candidates_.size();
}

bool JsepCandidateCollection::HasCandidate(
    const IceCandidateInterface* candidate) const {
  bool ret = false;
  for (std::vector<JsepIceCandidate*>::const_iterator it = candidates_.begin();
       it != candidates_.end(); ++it) {
    if ((*it)->sdp_mid() == candidate->sdp_mid() &&
        (*it)->sdp_mline_index() == candidate->sdp_mline_index() &&
        (*it)->candidate().IsEquivalent(candidate->candidate())) {
      ret = true;
      break;
    }
  }
  return ret;
}

void JsepCandidateCollection::add(JsepIceCandidate* candidate) {
  candidates_.push_back(candidate);
}

const IceCandidateInterface* JsepCandidateCollection::at(size_t index) const {
  return candidates_[index];
}

size_t JsepCandidateCollection::remove(const cricket::Candidate& candidate) {
  auto iter = std::find_if(candidates_.begin(), candidates_.end(),
                           [candidate](JsepIceCandidate* c) {
                             return candidate.MatchesForRemoval(c->candidate());
                           });
  if (iter != candidates_.end()) {
    delete *iter;
    candidates_.erase(iter);
    return 1;
  }
  return 0;
}

}  // namespace webrtc
