/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICELOGGER_H_
#define P2P_BASE_ICELOGGER_H_

#include <map>
#include <memory>
#include <string>

#include "logging/ice_log/icelogtype.h"
#include "p2p/base/candidate.h"
#include "p2p/base/port.h"

namespace webrtc {

#ifdef ENABLE_ICE_LOG_WITH_PROTOBUF
class FileWrapper;
#endif

namespace icelog {

// IceCandidateId and IceConnectionId: dedicated log identifiers for
// candidates/connections
class IceCandidateId final : public LogIdentifier,
                             public Comparable<IceCandidateId> {
 public:
  IceCandidateId() {}
  explicit IceCandidateId(const std::string& id) : LogIdentifier(id) {}

  using LogIdentifier::Compare;
  CompareResult Compare(const IceCandidateId& other) const override;
};

class IceConnectionId final : public LogIdentifier,
                              public Comparable<IceConnectionId> {
 public:
  IceConnectionId() {}
  explicit IceConnectionId(const std::string& id) : LogIdentifier(id) {}
  explicit IceConnectionId(cricket::Connection* conn) {
    set_id(conn->local_candidate().id() + ":" + conn->remote_candidate().id());
  }
  IceConnectionId(const IceCandidateId& local_cand_id,
                  const IceCandidateId& remote_cand_id) {
    set_id(local_cand_id.id() + ":" + remote_cand_id.id());
  }

  using LogIdentifier::Compare;
  CompareResult Compare(const IceConnectionId& other) const override;
};

// stringified enum classes
DECLARE_ENUMERATED_LOG_OBJECT(IceCandidateContent,
                              content,
                              kAudio,
                              kVideo,
                              kData);
DECLARE_ENUMERATED_LOG_OBJECT(IceCandidateProtocol,
                              protocol,
                              kUdp,
                              kTcp,
                              kSsltcp,
                              kTls);
DECLARE_ENUMERATED_LOG_OBJECT(IceCandidateType,
                              type,
                              kLocal,
                              kStun,
                              kPrflx,
                              kRelay);
DECLARE_ENUMERATED_LOG_OBJECT(IceCandidateNetwork, network, kWlan, kCell);

// IceCandidateProperty and IceConnectionProperty
class IceCandidateProperty final : public LogObject<true> {
 public:
  IceCandidateProperty() = delete;
  IceCandidateProperty(const cricket::Port& port,
                       const cricket::Candidate& c,
                       bool is_remote);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalDataInConstructor() override;

  IceCandidateId id() const { return id_; }
  bool is_remote() const { return is_remote_; }

  ~IceCandidateProperty() {}

  ADD_UNBOXED_DATA_WITHOUT_SETTER(IceCandidateId, id);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(IceCandidateType, type);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(IceCandidateContent, content);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(IceCandidateProtocol, protocol);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(IceCandidateNetwork, network);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(bool, is_remote);
};

// stringified enum classes
DECLARE_ENUMERATED_LOG_OBJECT(IceConnectionState,
                              state,
                              kInactive,
                              kWritable,
                              kWriteUnreliable,
                              kWriteInit,
                              kWriteTimeout,
                              kSentCheck,
                              kReceivedCheck,
                              kSentCheckResponse,
                              kReceivedCheckResponse,
                              kSelected);

class IceConnectionProperty final : public LogObject<true> {
 public:
  IceConnectionProperty();
  IceConnectionProperty(const IceCandidateProperty& local_candidate_property,
                        const IceCandidateProperty& remote_candidate_property);

  // from HasUnboxedInternalData
  /*virtual*/ void BoxInternalDataInConstructor() override;

  const IceCandidateProperty* local_candidate_property() {
    return local_candidate_property_;
  }
  const IceCandidateProperty* remote_candidate_property() {
    return remote_candidate_property_;
  }

  ADD_UNBOXED_DATA_WITHOUT_SETTER(IceConnectionId, id);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(const IceCandidateProperty*,
                                  local_candidate_property);
  ADD_UNBOXED_DATA_WITHOUT_SETTER(const IceCandidateProperty*,
                                  remote_candidate_property);
  ADD_UNBOXED_DATA_AND_DECLARE_SETTER(IceConnectionState, state)
};

// IceLogger is the single entry point for ICE structured log
class IceLogger {
 public:
  // Singleton, constructor and destructor private
  static IceLogger* Instance();

  LogEvent* CreateLogEventAndAddToEventPool(const LogEventType& type);

  IceCandidateId RegisterCandidate(cricket::Port* port,
                                   const cricket::Candidate& c,
                                   bool is_remote);
  IceConnectionId RegisterConnection(cricket::Connection* conn);
  void LogCandidateGathered(cricket::Port* port, const cricket::Candidate& c);
  void LogConnectionCreated(cricket::Connection* conn);
  void LogConnectionStateChange(cricket::Connection* conn,
                                IceConnectionState::Value prev_state,
                                IceConnectionState::Value cur_state);
  void LogConnectionPingResponseReceived(cricket::Connection* conn);
  void LogConnectionReselected(cricket::Connection* conn_old,
                               cricket::Connection* conn_new);

  void Log(const StructuredForm& structured_data);

 private:
  IceLogger();
  ~IceLogger();
  // the logger creates and has the unique ownership of all property objects
  std::map<IceCandidateId, std::unique_ptr<IceCandidateProperty>>
      candidate_property_by_id_;
  std::map<IceConnectionId, std::unique_ptr<IceConnectionProperty>>
      connection_property_by_id_;
  LogHookPool* hook_pool_;
  LogEventPool* event_pool_;

#ifdef ENABLE_ICE_LOG_WITH_PROTOBUF
  std::unique_ptr<FileWrapper> output_file_;
#endif
};

}  // namespace icelog

}  // namespace webrtc

#endif  // P2P_BASE_ICELOGGER_H_
