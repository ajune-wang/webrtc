/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "pc/simulcastdescription.h"
#include "rtc_base/checks.h"

namespace cricket {

SimulcastLayer::SimulcastLayer(const std::string& id, bool paused)
    : rid{id}, is_paused{paused} {
  // TODO(amithi): validate rid format or validate refers to existing rid.
  RTC_DCHECK(!id.empty());
}

void SimulcastLayerList::AddLayer(const SimulcastLayer& layer) {
  list_.push_back({layer});
}

void SimulcastLayerList::AddLayerWithAlternatives(
    const std::vector<SimulcastLayer>& rids) {
  RTC_DCHECK(!rids.empty());
  list_.push_back(rids);
}

const std::vector<SimulcastLayer>& SimulcastLayerList::operator[](
    size_t index) const {
  RTC_DCHECK_LT(index, list_.size());
  return list_[index];
}

bool SimulcastDescription::empty() const {
  return send_layers_.empty() && receive_layers_.empty();
}

}  // namespace cricket
