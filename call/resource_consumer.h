/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RESOURCE_CONSUMER_H_
#define CALL_RESOURCE_CONSUMER_H_

#include <string>

namespace webrtc {
namespace adaptation {

class ResourceConsumerConfiguration;

class ResourceConsumer final {
 public:
  ResourceConsumer(std::string name,
                   ResourceConsumerConfiguration* configuration);
  ~ResourceConsumer();

  std::string name() const;
  ResourceConsumerConfiguration* configuration() const;
  void SetConfiguration(ResourceConsumerConfiguration* configuration);

  std::string ToString() const;

 private:
  std::string name_;
  ResourceConsumerConfiguration* configuration_;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_RESOURCE_CONSUMER_H_
