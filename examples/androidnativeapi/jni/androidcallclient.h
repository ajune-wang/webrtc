/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_ANDROIDNATIVEAPI_JNI_ANDROIDCALLCLIENT_H_
#define EXAMPLES_ANDROIDNATIVEAPI_JNI_ANDROIDCALLCLIENT_H_

#include <jni.h>

#include <memory>
#include <string>

#include "api/peerconnectioninterface.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc_examples {

class AndroidCallClient {
 public:
  AndroidCallClient();

  void Call(JNIEnv* env,
            const webrtc::JavaRef<jobject>& cls,
            const webrtc::JavaRef<jobject>& local_sink,
            const webrtc::JavaRef<jobject>& remote_sink);
  void Hangup(JNIEnv* env, const webrtc::JavaRef<jobject>& cls);
  // A helper method for Java code to delete this object. Calls delete this.
  void Delete(JNIEnv* env, const webrtc::JavaRef<jobject>& cls);

 private:
  class PCObserver;
  class CreateOfferObserver;
  class SetRemoteSessionDescriptionObserver;
  class SetLocalSessionDescriptionObserver;

  void CreatePeerConnectionFactory();
  void CreatePeerConnection();
  void Connect();

  const std::unique_ptr<PCObserver> pc_observer_;
  const rtc::scoped_refptr<CreateOfferObserver> create_offer_observer_;
  const rtc::scoped_refptr<SetRemoteSessionDescriptionObserver>
      set_remote_session_description_observer_;
  const rtc::scoped_refptr<SetLocalSessionDescriptionObserver>
      set_local_session_description_observer_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<rtc::Thread> signaling_thread_;

  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> local_sink_;
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> remote_sink_;
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
};

}  // namespace webrtc_examples

#endif  // EXAMPLES_ANDROIDNATIVEAPI_JNI_ANDROIDCALLCLIENT_H_
