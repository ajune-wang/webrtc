/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PUBLIC_DCSCTP_OPTIONS_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_OPTIONS_H_

#include <stddef.h>
#include <stdint.h>

namespace dcsctp {
struct DcSctpOptions {
  // The local port for which the socket is supposed to be bound to. Incoming
  // packets will be verified that they are sent to this port number and all
  // outgoing packets will have this port number as source port.
  int local_port = 5000;

  // The remote port to send packets to. All outgoing packets will have this
  // port number as destination port.
  int remote_port = 5000;

  // Maximum SCTP packet size. Note that on top of this, DTLS header (13 + 16 IV
  // + 20 HMAC + 12 padding/PL bytes), UDP header (8 bytes), IPv4 (60 bytes) or
  // IPv6 (40 bytes), and any lower layer protocols. For IPv6, it's important to
  // stay under the minimum MTU (1280) as fragmentation support in IPv6 is
  // limited.
  //
  // 1280 - 61 - 8 - 40 = 1171 -> 1170
  size_t mtu = 1170;

  // Maximum received window buffer size. This should be a bit larger than the
  // largest sized message you want to be able to receive. This essentially
  // limits the memory usage on the receive side. Note that memory is allocated
  // dynamically, and this represents the maximum amount of buffered data. The
  // actual memory usage of the library will be smaller in normal operation, and
  // will be larger than this due to other allocations and overhead if the
  // buffer is fully utilized.
  size_t max_receiver_window_buffer_size = 5 * 1024 * 1024;

  // Maximum send buffer size. It will not be possible to queue more data than
  // this before sending it.
  size_t max_send_buffer_size = 2 * 1024 * 1024;

  // Initial RTO value.
  int rto_initial_ms = 500;

  // Maximum RTO value.
  int rto_max_ms = 800;

  // Minimum RTO value.
  int rto_min_ms = 120;

  // T1-init timeout.
  int t1_init_timeout_ms = 1000;

  // T1-cookie timeout.
  int t1_cookie_timeout_ms = 1000;

  // T2-shutdown timeout.
  int t2_shutdown_timeout_ms = 1000;

  // Hearbeat interval (on idle connections only).
  int heartbeat_interval_ms = 5000;

  // The maximum time when a SACK will be sent from the arrival of an
  // unacknowledged packet. Whatever is smallest of RTO/2 and this will be used.
  int delayed_ack_max_timeout_ms = 200;

  // Do slow start as TCP - double cwnd instead of increasing it by MTU.
  bool slow_start_tcp_style = true;

  // Maximum Data Retransmit Attempts (per DATA chunk).
  int max_retransmissions = 10;

  // Max.Init.Retransmits (From RFC)
  int max_init_retransmits = 10;

  // RFC3758 Partial Reliability Extension
  bool enable_partial_reliability = true;

  // RFC8260 Stream Schedulers and User Message Interleaving
  bool enable_message_interleaving = false;

  // If RTO should be added to heartbeat_interval
  bool heartbeat_interval_include_rtt = true;

  // Disables SCTP packet crc32 verification. Useful when running with fuzzers.
  bool disable_checksum_verification = false;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_DCSCTP_OPTIONS_H_
