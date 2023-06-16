/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "api/function_view.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"

namespace webrtc {

// This file contains helper class used to process the elements of two or more
// sorted lists in timestamp order. The effect is the same as doing a merge step
// in the merge-sort algorithm but without copying the elements or modifying the
// lists.

namespace event_processor_impl {
// Interface to allow "merging" lists of different types. ProcessNext()
// processes the next unprocessed element in the list. IsEmpty() checks if all
// elements have been processed. GetNextTime returns the timestamp of the next
// unprocessed element.
class ProcessableEventListInterface {
 public:
  virtual ~ProcessableEventListInterface() = default;
  virtual void ProcessNext() = 0;
  virtual bool IsEmpty() const = 0;
  virtual int64_t GetNextTime() const = 0;
  virtual int GetTypeOrder() const = 0;
  virtual absl::optional<uint16_t> GetTransportSeqNum() const = 0;
  virtual int GetInsertionOrder() const = 0;
};

// ProcessableEventList encapsulates a list of events and a function that will
// be applied to each element of the list.
template <typename Iterator, typename T>
class ProcessableEventList : public ProcessableEventListInterface {
 public:
  ProcessableEventList(Iterator begin,
                       Iterator end,
                       std::function<void(const T&)> f,
                       int type_order,
                       std::function<absl::optional<uint16_t>(const T&)>
                           transport_seq_num_accessor,
                       int insertion_order)
      : begin_(begin),
        end_(end),
        f_(f),
        type_order_(type_order),
        transport_seq_num_accessor_(transport_seq_num_accessor),
        insertion_order_(insertion_order) {}

  void ProcessNext() override {
    RTC_DCHECK(!IsEmpty());
    f_(*begin_);
    ++begin_;
  }

  bool IsEmpty() const override { return begin_ == end_; }

  int64_t GetNextTime() const override {
    RTC_DCHECK(!IsEmpty());
    return begin_->log_time_us();
  }

  int GetTypeOrder() const override { return type_order_; }

  absl::optional<uint16_t> GetTransportSeqNum() const override {
    RTC_DCHECK(!IsEmpty());
    return transport_seq_num_accessor_(*begin_);
  }

  int GetInsertionOrder() const override { return insertion_order_; }

 private:
  Iterator begin_;
  Iterator end_;
  std::function<void(const T&)> f_;
  int type_order_;
  std::function<absl::optional<uint16_t>(const T&)> transport_seq_num_accessor_;
  int insertion_order_;
};

}  // namespace event_processor_impl

// The RTC event log only uses millisecond precision timestamps
// and doesn't preserve order between events in different batches.
// This is heuristic
enum class TypeOrder {
  Start,
  // Connectivity and stream configurations before incoming packets
  StreamConfig,
  IceCondidateConfig,
  IceCandidateEvent,
  DtlsTransportState,
  DtlsWritable,
  RouteChange,
  // Incoming packets
  RtpIn,
  RtcpIn,
  GenericPacketIn,
  GenericAckIn,
  // BWE depends on incoming feedback (send side estimation)
  // or incoming media packets (receive side estimation).
  // Delay-based BWE depends on probe results.
  // Loss-based BWE depends on delay-based BWE.
  // Loss-based BWE may trigger new probes.
  BweRemoteEstimate,
  BweProbeFailure,
  BweProbeSuccess,
  BweDelayBased,
  BweLossBased,
  BweProbeCreated,
  // General processing events. No obvious order.
  AudioNetworkAdaptation,
  NetEqSetMinDelay,
  AudioPlayout,
  FrameDecoded,
  // Outgoing packets and feedback depends on BWE and might depend on
  // processing.
  RtpOut,
  RtcpOut,
  GenericPacketOut,
  // Alr is updated after a packet is sent.
  AlrState,
  Stop,
};

template <typename T>
class TieBreaker {
  static_assert(sizeof(T) != sizeof(T),
                "Specialize TieBreaker to define an order for the event type.");
};

template <>
class TieBreaker<LoggedStartEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::Start);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedStartEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedStopEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::Start);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedStopEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioRecvConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioRecvConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioSendConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioSendConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedVideoRecvConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedVideoRecvConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedVideoSendConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedVideoSendConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedIceCandidatePairConfig> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::IceCondidateConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedIceCandidatePairConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedIceCandidatePairEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::IceCandidateEvent);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedIceCandidatePairEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedDtlsTransportState> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::DtlsTransportState);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedDtlsTransportState&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedDtlsWritableState> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::DtlsWritable);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedDtlsWritableState&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRouteChangeEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RouteChange);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRouteChangeEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRemoteEstimateEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweRemoteEstimate);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRemoteEstimateEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweProbeFailureEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweProbeFailure);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweProbeFailureEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweProbeSuccessEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweProbeSuccess);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweProbeSuccessEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweDelayBasedUpdate> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::BweDelayBased);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweDelayBasedUpdate&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweLossBasedUpdate> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::BweLossBased);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweLossBasedUpdate&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweProbeClusterCreatedEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweProbeCreated);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweProbeClusterCreatedEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioNetworkAdaptationEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::AudioNetworkAdaptation);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioNetworkAdaptationEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedNetEqSetMinimumDelayEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::NetEqSetMinDelay);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedNetEqSetMinimumDelayEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioPlayoutEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::AudioPlayout);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioPlayoutEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedFrameDecoded> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::FrameDecoded);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedFrameDecoded&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedGenericPacketReceived> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::GenericPacketIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedGenericPacketReceived&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedGenericAckReceived> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::GenericAckIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedGenericAckReceived&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedGenericPacketSent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::GenericPacketOut);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedGenericPacketSent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtpPacket> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtpIn
                                : TypeOrder::RtpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtpPacket& p) {
    return p.header.extension.hasTransportSequenceNumber
               ? p.header.extension.transportSequenceNumber
               : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedPacketInfo> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtpIn
                                : TypeOrder::RtpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedPacketInfo& p) {
    return p.has_transport_seq_no ? p.transport_seq_no
                                  : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtpPacketIncoming> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtpIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtpPacketIncoming& p) {
    return p.rtp.header.extension.hasTransportSequenceNumber
               ? p.rtp.header.extension.transportSequenceNumber
               : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtpPacketOutgoing> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtpOut);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtpPacketOutgoing& p) {
    return p.rtp.header.extension.hasTransportSequenceNumber
               ? p.rtp.header.extension.transportSequenceNumber
               : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketIncoming> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtcpIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketIncoming&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketOutgoing> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtcpOut);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketOutgoing&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketTransportFeedback> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtcpIn
                                : TypeOrder::RtcpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketTransportFeedback&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketReceiverReport> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtcpIn
                                : TypeOrder::RtcpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketReceiverReport&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAlrStateEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::AlrState);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAlrStateEvent&) {
    return absl::optional<uint16_t>();
  }
};

// Helper class used to "merge" two or more lists of ordered RtcEventLog events
// so that they can be treated as a single ordered list. Since the individual
// lists may have different types, we need to access the lists via pointers to
// the common base class.
//
// Usage example:
// ParsedRtcEventLogNew log;
// auto incoming_handler = [] (LoggedRtcpPacketIncoming elem) { ... };
// auto outgoing_handler = [] (LoggedRtcpPacketOutgoing elem) { ... };
//
// RtcEventProcessor processor;
// processor.AddEvents(log.incoming_rtcp_packets(),
//                     incoming_handler);
// processor.AddEvents(log.outgoing_rtcp_packets(),
//                     outgoing_handler);
// processor.ProcessEventsInOrder();
class RtcEventProcessor {
 public:
  RtcEventProcessor();
  ~RtcEventProcessor();
  // The elements of each list is processed in the index order. To process all
  // elements in all lists in timestamp order, each list needs to be sorted in
  // timestamp order prior to insertion.
  // N.B. `iterable` is not owned by RtcEventProcessor. The caller must ensure
  // that the iterable outlives RtcEventProcessor and it must not be modified
  // until processing has finished.
  template <typename Iterable>
  void AddEvents(
      const Iterable& iterable,
      std::function<void(const typename Iterable::value_type&)> handler) {
    using ValueType =
        typename std::remove_const<typename Iterable::value_type>::type;
    AddEvents(iterable, handler, TieBreaker<ValueType>::type_order,
              TieBreaker<ValueType>::transport_seq_num_accessor,
              num_insertions_);
  }

  template <typename Iterable>
  void AddEvents(
      const Iterable& iterable,
      std::function<void(const typename Iterable::value_type&)> handler,
      PacketDirection direction) {
    using ValueType =
        typename std::remove_const<typename Iterable::value_type>::type;
    AddEvents(iterable, handler, TieBreaker<ValueType>::type_order(direction),
              TieBreaker<ValueType>::transport_seq_num_accessor,
              num_insertions_);
  }

  template <typename Iterable>
  void AddEvents(
      const Iterable& iterable,
      std::function<void(const typename Iterable::value_type&)> handler,
      int type_order,
      std::function<absl::optional<uint16_t>(
          const typename Iterable::value_type&)> transport_seq_num_accessor,
      int insertion_order) {
    if (iterable.begin() == iterable.end())
      return;
    num_insertions_++;
    event_lists_.push_back(
        std::make_unique<event_processor_impl::ProcessableEventList<
            typename Iterable::const_iterator, typename Iterable::value_type>>(
            iterable.begin(), iterable.end(), handler, type_order,
            transport_seq_num_accessor, insertion_order));
  }

  void ProcessEventsInOrder();

 private:
  using ListPtrType =
      std::unique_ptr<event_processor_impl::ProcessableEventListInterface>;
  // Comparison function to make `event_lists_` into a min heap.
  static bool Cmp(const ListPtrType& a, const ListPtrType& b);

  std::vector<ListPtrType> event_lists_;
  int num_insertions_ = 0;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_
