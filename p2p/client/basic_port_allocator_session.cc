/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/client/basic_port_allocator_session.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_port.h"
#include "p2p/base/tcp_port.h"
#include "p2p/base/turn_port.h"
#include "p2p/base/udp_port.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

using rtc::CreateRandomId;

namespace cricket {
namespace {

BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd)
    : PortAllocatorSession(content_name,
                           component,
                           ice_ufrag,
                           ice_pwd,
                           allocator->flags()),
      allocator_(allocator),
      network_thread_(rtc::Thread::Current()),
      socket_factory_(allocator->socket_factory()),
      allocation_started_(false),
      network_manager_started_(false),
      allocation_sequences_created_(false),
      turn_port_prune_policy_(allocator->turn_port_prune_policy()) {
  allocator_->network_manager()->SignalNetworksChanged.connect(
      this, &BasicPortAllocatorSession::OnNetworksChanged);
  allocator_->network_manager()->StartUpdating();
}

BasicPortAllocatorSession::~BasicPortAllocatorSession() {
  RTC_DCHECK_RUN_ON(network_thread_);
  allocator_->network_manager()->StopUpdating();
  if (network_thread_ != NULL)
    network_thread_->Clear(this);

  for (uint32_t i = 0; i < sequences_.size(); ++i) {
    // AllocationSequence should clear it's map entry for turn ports before
    // ports are destroyed.
    sequences_[i]->Clear();
  }

  std::vector<PortData>::iterator it;
  for (it = ports_.begin(); it != ports_.end(); it++)
    delete it->port();

  for (uint32_t i = 0; i < configs_.size(); ++i)
    delete configs_[i];

  for (uint32_t i = 0; i < sequences_.size(); ++i)
    delete sequences_[i];
}

BasicPortAllocator* BasicPortAllocatorSession::allocator() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return allocator_;
}

void BasicPortAllocatorSession::SetCandidateFilter(uint32_t filter) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (filter == candidate_filter_) {
    return;
  }
  uint32_t prev_filter = candidate_filter_;
  candidate_filter_ = filter;
  for (PortData& port_data : ports_) {
    if (port_data.error() || port_data.pruned()) {
      continue;
    }
    PortData::State cur_state = port_data.state();
    bool found_signalable_candidate = false;
    bool found_pairable_candidate = false;
    cricket::Port* port = port_data.port();
    for (const auto& c : port->Candidates()) {
      if (!IsStopped() && !IsAllowedByCandidateFilter(c, prev_filter) &&
          IsAllowedByCandidateFilter(c, filter)) {
        // This candidate was not signaled because of not matching the previous
        // filter (see OnCandidateReady below). Let the Port to fire the signal
        // again.
        //
        // Note that
        //  1) we would need the Port to enter the state of in-progress of
        //     gathering to have candidates signaled;
        //
        //  2) firing the signal would also let the session set the port ready
        //     if needed, so that we could form candidate pairs with candidates
        //     from this port;
        //
        //  *  See again OnCandidateReady below for 1) and 2).
        //
        //  3) we only try to resurface candidates if we have not stopped
        //     getting ports, which is always true for the continual gathering.
        if (!found_signalable_candidate) {
          found_signalable_candidate = true;
          port_data.set_state(PortData::STATE_INPROGRESS);
        }
        port->SignalCandidateReady(port, c);
      }

      if (CandidatePairable(c, port)) {
        found_pairable_candidate = true;
      }
    }
    // Restore the previous state.
    port_data.set_state(cur_state);
    // Setting a filter may cause a ready port to become non-ready
    // if it no longer has any pairable candidates.
    //
    // Note that we only set for the negative case here, since a port would be
    // set to have pairable candidates when it signals a ready candidate, which
    // requires the port is still in the progress of gathering/surfacing
    // candidates, and would be done in the firing of the signal above.
    if (!found_pairable_candidate) {
      port_data.set_has_pairable_candidate(false);
    }
  }
}

void BasicPortAllocatorSession::StartGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  state_ = SessionState::GATHERING;
  if (!socket_factory_) {
    owned_socket_factory_.reset(
        new rtc::BasicPacketSocketFactory(network_thread_));
    socket_factory_ = owned_socket_factory_.get();
  }

  network_thread_->Post(RTC_FROM_HERE, this, MSG_CONFIG_START);

  RTC_LOG(LS_INFO) << "Start getting ports with turn_port_prune_policy "
                   << turn_port_prune_policy_;
}

void BasicPortAllocatorSession::StopGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  ClearGettingPorts();
  // Note: this must be called after ClearGettingPorts because both may set the
  // session state and we should set the state to STOPPED.
  state_ = SessionState::STOPPED;
}

void BasicPortAllocatorSession::ClearGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  network_thread_->Clear(this, MSG_ALLOCATE);
  for (uint32_t i = 0; i < sequences_.size(); ++i) {
    sequences_[i]->Stop();
  }
  network_thread_->Post(RTC_FROM_HERE, this, MSG_CONFIG_STOP);
  state_ = SessionState::CLEARED;
}

bool BasicPortAllocatorSession::IsGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_ == SessionState::GATHERING;
}

bool BasicPortAllocatorSession::IsCleared() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_ == SessionState::CLEARED;
}

bool BasicPortAllocatorSession::IsStopped() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_ == SessionState::STOPPED;
}

std::vector<rtc::Network*> BasicPortAllocatorSession::GetFailedNetworks() {
  RTC_DCHECK_RUN_ON(network_thread_);

  std::vector<rtc::Network*> networks = GetNetworks();

  // A network interface may have both IPv4 and IPv6 networks. Only if
  // neither of the networks has any connections, the network interface
  // is considered failed and need to be regathered on.
  std::set<std::string> networks_with_connection;
  for (const PortData& data : ports_) {
    Port* port = data.port();
    if (!port->connections().empty()) {
      networks_with_connection.insert(port->Network()->name());
    }
  }

  networks.erase(
      std::remove_if(networks.begin(), networks.end(),
                     [networks_with_connection](rtc::Network* network) {
                       // If a network does not have any connection, it is
                       // considered failed.
                       return networks_with_connection.find(network->name()) !=
                              networks_with_connection.end();
                     }),
      networks.end());
  return networks;
}

void BasicPortAllocatorSession::RegatherOnFailedNetworks() {
  RTC_DCHECK_RUN_ON(network_thread_);

  // Find the list of networks that have no connection.
  std::vector<rtc::Network*> failed_networks = GetFailedNetworks();
  if (failed_networks.empty()) {
    return;
  }

  RTC_LOG(LS_INFO) << "Regather candidates on failed networks";

  // Mark a sequence as "network failed" if its network is in the list of failed
  // networks, so that it won't be considered as equivalent when the session
  // regathers ports and candidates.
  for (AllocationSequence* sequence : sequences_) {
    if (!sequence->network_failed() &&
        absl::c_linear_search(failed_networks, sequence->network())) {
      sequence->set_network_failed();
    }
  }

  bool disable_equivalent_phases = true;
  Regather(failed_networks, disable_equivalent_phases,
           IceRegatheringReason::NETWORK_FAILURE);
}

void BasicPortAllocatorSession::Regather(
    const std::vector<rtc::Network*>& networks,
    bool disable_equivalent_phases,
    IceRegatheringReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);
  // Remove ports from being used locally and send signaling to remove
  // the candidates on the remote side.
  std::vector<PortData*> ports_to_prune = GetUnprunedPorts(networks);
  if (!ports_to_prune.empty()) {
    RTC_LOG(LS_INFO) << "Prune " << ports_to_prune.size() << " ports";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }

  if (allocation_started_ && network_manager_started_ && !IsStopped()) {
    SignalIceRegathering(this, reason);

    DoAllocate(disable_equivalent_phases);
  }
}

void BasicPortAllocatorSession::GetCandidateStatsFromReadyPorts(
    CandidateStatsList* candidate_stats_list) const {
  auto ports = ReadyPorts();
  for (auto* port : ports) {
    auto candidates = port->Candidates();
    for (const auto& candidate : candidates) {
      CandidateStats candidate_stats(allocator_->SanitizeCandidate(candidate));
      port->GetStunStats(&candidate_stats.stun_stats);
      candidate_stats_list->push_back(std::move(candidate_stats));
    }
  }
}

void BasicPortAllocatorSession::SetStunKeepaliveIntervalForReadyPorts(
    const absl::optional<int>& stun_keepalive_interval) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto ports = ReadyPorts();
  for (PortInterface* port : ports) {
    // The port type and protocol can be used to identify different subclasses
    // of Port in the current implementation. Note that a TCPPort has the type
    // LOCAL_PORT_TYPE but uses the protocol PROTO_TCP.
    if (port->Type() == STUN_PORT_TYPE ||
        (port->Type() == LOCAL_PORT_TYPE && port->GetProtocol() == PROTO_UDP)) {
      static_cast<UDPPort*>(port)->set_stun_keepalive_delay(
          stun_keepalive_interval);
    }
  }
}

std::vector<PortInterface*> BasicPortAllocatorSession::ReadyPorts() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<PortInterface*> ret;
  for (const PortData& data : ports_) {
    if (data.ready()) {
      ret.push_back(data.port());
    }
  }
  return ret;
}

std::vector<Candidate> BasicPortAllocatorSession::ReadyCandidates() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<Candidate> candidates;
  for (const PortData& data : ports_) {
    if (!data.ready()) {
      continue;
    }
    GetCandidatesFromPort(data, &candidates);
  }
  return candidates;
}

void BasicPortAllocatorSession::GetCandidatesFromPort(
    const PortData& data,
    std::vector<Candidate>* candidates) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_CHECK(candidates != nullptr);
  for (const Candidate& candidate : data.port()->Candidates()) {
    if (!CheckCandidateFilter(candidate)) {
      continue;
    }
    candidates->push_back(allocator_->SanitizeCandidate(candidate));
  }
}

bool BasicPortAllocatorSession::CandidatesAllocationDone() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  // Done only if all required AllocationSequence objects
  // are created.
  if (!allocation_sequences_created_) {
    return false;
  }

  // Check that all port allocation sequences are complete (not running).
  if (absl::c_any_of(sequences_, [](const AllocationSequence* sequence) {
        return sequence->state() == AllocationSequence::kRunning;
      })) {
    return false;
  }

  // If all allocated ports are no longer gathering, session must have got all
  // expected candidates. Session will trigger candidates allocation complete
  // signal.
  return absl::c_none_of(
      ports_, [](const PortData& port) { return port.inprogress(); });
}

void BasicPortAllocatorSession::OnMessage(rtc::Message* message) {
  switch (message->message_id) {
    case MSG_CONFIG_START:
      GetPortConfigurations();
      break;
    case MSG_CONFIG_READY:
      OnConfigReady(static_cast<PortConfiguration*>(message->pdata));
      break;
    case MSG_ALLOCATE:
      OnAllocate();
      break;
    case MSG_SEQUENCEOBJECTS_CREATED:
      OnAllocationSequenceObjectsCreated();
      break;
    case MSG_CONFIG_STOP:
      OnConfigStop();
      break;
    default:
      RTC_NOTREACHED();
  }
}

void BasicPortAllocatorSession::UpdateIceParametersInternal() {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (PortData& port : ports_) {
    port.port()->set_content_name(content_name());
    port.port()->SetIceParameters(component(), ice_ufrag(), ice_pwd());
  }
}

void BasicPortAllocatorSession::GetPortConfigurations() {
  RTC_DCHECK_RUN_ON(network_thread_);

  PortConfiguration* config =
      new PortConfiguration(allocator_->stun_servers(), username(), password());

  for (const RelayServerConfig& turn_server : allocator_->turn_servers()) {
    config->AddRelay(turn_server);
  }
  ConfigReady(config);
}

void BasicPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  network_thread_->Post(RTC_FROM_HERE, this, MSG_CONFIG_READY, config);
}

// Adds a configuration to the list.
void BasicPortAllocatorSession::OnConfigReady(PortConfiguration* config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (config) {
    configs_.push_back(config);
  }

  AllocatePorts();
}

void BasicPortAllocatorSession::OnConfigStop() {
  RTC_DCHECK_RUN_ON(network_thread_);

  // If any of the allocated ports have not completed the candidates allocation,
  // mark those as error. Since session doesn't need any new candidates
  // at this stage of the allocation, it's safe to discard any new candidates.
  bool send_signal = false;
  for (std::vector<PortData>::iterator it = ports_.begin(); it != ports_.end();
       ++it) {
    if (it->inprogress()) {
      // Updating port state to error, which didn't finish allocating candidates
      // yet.
      it->set_state(PortData::STATE_ERROR);
      send_signal = true;
    }
  }

  // Did we stop any running sequences?
  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end() && !send_signal; ++it) {
    if ((*it)->state() == AllocationSequence::kStopped) {
      send_signal = true;
    }
  }

  // If we stopped anything that was running, send a done signal now.
  if (send_signal) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::AllocatePorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  network_thread_->Post(RTC_FROM_HERE, this, MSG_ALLOCATE);
}

void BasicPortAllocatorSession::OnAllocate() {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (network_manager_started_ && !IsStopped()) {
    bool disable_equivalent_phases = true;
    DoAllocate(disable_equivalent_phases);
  }

  allocation_started_ = true;
}

std::vector<rtc::Network*> BasicPortAllocatorSession::GetNetworks() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<rtc::Network*> networks;
  rtc::NetworkManager* network_manager = allocator_->network_manager();
  RTC_DCHECK(network_manager != nullptr);
  // If the network permission state is BLOCKED, we just act as if the flag has
  // been passed in.
  if (network_manager->enumeration_permission() ==
      rtc::NetworkManager::ENUMERATION_BLOCKED) {
    set_flags(flags() | PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
  }
  // If the adapter enumeration is disabled, we'll just bind to any address
  // instead of specific NIC. This is to ensure the same routing for http
  // traffic by OS is also used here to avoid any local or public IP leakage
  // during stun process.
  if (flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) {
    network_manager->GetAnyAddressNetworks(&networks);
  } else {
    network_manager->GetNetworks(&networks);
    // If network enumeration fails, use the ANY address as a fallback, so we
    // can at least try gathering candidates using the default route chosen by
    // the OS. Or, if the PORTALLOCATOR_ENABLE_ANY_ADDRESS_PORTS flag is
    // set, we'll use ANY address candidates either way.
    if (networks.empty() || flags() & PORTALLOCATOR_ENABLE_ANY_ADDRESS_PORTS) {
      network_manager->GetAnyAddressNetworks(&networks);
    }
  }
  // Filter out link-local networks if needed.
  if (flags() & PORTALLOCATOR_DISABLE_LINK_LOCAL_NETWORKS) {
    NetworkFilter link_local_filter(
        [](rtc::Network* network) { return IPIsLinkLocal(network->prefix()); },
        "link-local");
    FilterNetworks(&networks, link_local_filter);
  }
  // Do some more filtering, depending on the network ignore mask and "disable
  // costly networks" flag.
  NetworkFilter ignored_filter(
      [this](rtc::Network* network) {
        return allocator_->network_ignore_mask() & network->type();
      },
      "ignored");
  FilterNetworks(&networks, ignored_filter);
  if (flags() & PORTALLOCATOR_DISABLE_COSTLY_NETWORKS) {
    uint16_t lowest_cost = rtc::kNetworkCostMax;
    for (rtc::Network* network : networks) {
      // Don't determine the lowest cost from a link-local network.
      // On iOS, a device connected to the computer will get a link-local
      // network for communicating with the computer, however this network can't
      // be used to connect to a peer outside the network.
      if (rtc::IPIsLinkLocal(network->GetBestIP())) {
        continue;
      }
      lowest_cost = std::min<uint16_t>(lowest_cost, network->GetCost());
    }
    NetworkFilter costly_filter(
        [lowest_cost](rtc::Network* network) {
          return network->GetCost() > lowest_cost + rtc::kNetworkCostLow;
        },
        "costly");
    FilterNetworks(&networks, costly_filter);
  }
  // Lastly, if we have a limit for the number of IPv6 network interfaces (by
  // default, it's 5), remove networks to ensure that limit is satisfied.
  //
  // TODO(deadbeef): Instead of just taking the first N arbitrary IPv6
  // networks, we could try to choose a set that's "most likely to work". It's
  // hard to define what that means though; it's not just "lowest cost".
  // Alternatively, we could just focus on making our ICE pinging logic smarter
  // such that this filtering isn't necessary in the first place.
  int ipv6_networks = 0;
  for (auto it = networks.begin(); it != networks.end();) {
    if ((*it)->prefix().family() == AF_INET6) {
      if (ipv6_networks >= allocator_->max_ipv6_networks()) {
        it = networks.erase(it);
        continue;
      } else {
        ++ipv6_networks;
      }
    }
    ++it;
  }
  return networks;
}

// For each network, see if we have a sequence that covers it already.  If not,
// create a new sequence to create the appropriate ports.
void BasicPortAllocatorSession::DoAllocate(bool disable_equivalent) {
  RTC_DCHECK_RUN_ON(network_thread_);
  bool done_signal_needed = false;
  std::vector<rtc::Network*> networks = GetNetworks();
  if (networks.empty()) {
    RTC_LOG(LS_WARNING)
        << "Machine has no networks; no ports will be allocated";
    done_signal_needed = true;
  } else {
    RTC_LOG(LS_INFO) << "Allocate ports on " << networks.size() << " networks";
    PortConfiguration* config = configs_.empty() ? nullptr : configs_.back();
    for (uint32_t i = 0; i < networks.size(); ++i) {
      uint32_t sequence_flags = flags();
      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // If all the ports are disabled we should just fire the allocation
        // done event and return.
        done_signal_needed = true;
        break;
      }

      if (!config || config->relays.empty()) {
        // No relay ports specified in this config.
        sequence_flags |= PORTALLOCATOR_DISABLE_RELAY;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6) &&
          networks[i]->GetBestIP().family() == AF_INET6) {
        // Skip IPv6 networks unless the flag's been set.
        continue;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6_ON_WIFI) &&
          networks[i]->GetBestIP().family() == AF_INET6 &&
          networks[i]->type() == rtc::ADAPTER_TYPE_WIFI) {
        // Skip IPv6 Wi-Fi networks unless the flag's been set.
        continue;
      }

      if (disable_equivalent) {
        // Disable phases that would only create ports equivalent to
        // ones that we have already made.
        DisableEquivalentPhases(networks[i], config, &sequence_flags);

        if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
          // New AllocationSequence would have nothing to do, so don't make it.
          continue;
        }
      }

      AllocationSequence* sequence =
          new AllocationSequence(this, networks[i], config, sequence_flags);
      sequence->SignalPortAllocationComplete.connect(
          this, &BasicPortAllocatorSession::OnPortAllocationComplete);
      sequence->Init();
      sequence->Start();
      sequences_.push_back(sequence);
      done_signal_needed = true;
    }
  }
  if (done_signal_needed) {
    network_thread_->Post(RTC_FROM_HERE, this, MSG_SEQUENCEOBJECTS_CREATED);
  }
}

void BasicPortAllocatorSession::OnNetworksChanged() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<rtc::Network*> networks = GetNetworks();
  std::vector<rtc::Network*> failed_networks;
  for (AllocationSequence* sequence : sequences_) {
    // Mark the sequence as "network failed" if its network is not in
    // |networks|.
    if (!sequence->network_failed() &&
        !absl::c_linear_search(networks, sequence->network())) {
      sequence->OnNetworkFailed();
      failed_networks.push_back(sequence->network());
    }
  }
  std::vector<PortData*> ports_to_prune = GetUnprunedPorts(failed_networks);
  if (!ports_to_prune.empty()) {
    RTC_LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                     << " ports because their networks were gone";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }

  if (allocation_started_ && !IsStopped()) {
    if (network_manager_started_) {
      // If the network manager has started, it must be regathering.
      SignalIceRegathering(this, IceRegatheringReason::NETWORK_CHANGE);
    }
    bool disable_equivalent_phases = true;
    DoAllocate(disable_equivalent_phases);
  }

  if (!network_manager_started_) {
    RTC_LOG(LS_INFO) << "Network manager has started";
    network_manager_started_ = true;
  }
}

void BasicPortAllocatorSession::DisableEquivalentPhases(
    rtc::Network* network,
    PortConfiguration* config,
    uint32_t* flags) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (uint32_t i = 0; i < sequences_.size() &&
                       (*flags & DISABLE_ALL_PHASES) != DISABLE_ALL_PHASES;
       ++i) {
    sequences_[i]->DisableEquivalentPhases(network, config, flags);
  }
}

void BasicPortAllocatorSession::AddAllocatedPort(Port* port,
                                                 AllocationSequence* seq,
                                                 bool prepare_address) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!port)
    return;

  RTC_LOG(LS_INFO) << "Adding allocated port for " << content_name();
  port->set_content_name(content_name());
  port->set_component(component());
  port->set_generation(generation());
  if (allocator_->proxy().type != rtc::PROXY_NONE)
    port->set_proxy(allocator_->user_agent(), allocator_->proxy());
  port->set_send_retransmit_count_attribute(
      (flags() & PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE) != 0);

  PortData data(port, seq);
  ports_.push_back(data);

  port->SignalCandidateReady.connect(
      this, &BasicPortAllocatorSession::OnCandidateReady);
  port->SignalCandidateError.connect(
      this, &BasicPortAllocatorSession::OnCandidateError);
  port->SignalPortComplete.connect(this,
                                   &BasicPortAllocatorSession::OnPortComplete);
  port->SignalDestroyed.connect(this,
                                &BasicPortAllocatorSession::OnPortDestroyed);
  port->SignalPortError.connect(this, &BasicPortAllocatorSession::OnPortError);
  RTC_LOG(LS_INFO) << port->ToString() << ": Added port to allocator";

  if (prepare_address)
    port->PrepareAddress();
}

void BasicPortAllocatorSession::OnAllocationSequenceObjectsCreated() {
  RTC_DCHECK_RUN_ON(network_thread_);
  allocation_sequences_created_ = true;
  // Send candidate allocation complete signal if we have no sequences.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnCandidateReady(Port* port,
                                                 const Candidate& c) {
  RTC_DCHECK_RUN_ON(network_thread_);
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);
  RTC_LOG(LS_INFO) << port->ToString()
                   << ": Gathered candidate: " << c.ToSensitiveString();
  // Discarding any candidate signal if port allocation status is
  // already done with gathering.
  if (!data->inprogress()) {
    RTC_LOG(LS_WARNING)
        << "Discarding candidate because port is already done gathering.";
    return;
  }

  // Mark that the port has a pairable candidate, either because we have a
  // usable candidate from the port, or simply because the port is bound to the
  // any address and therefore has no host candidate. This will trigger the port
  // to start creating candidate pairs (connections) and issue connectivity
  // checks. If port has already been marked as having a pairable candidate,
  // do nothing here.
  // Note: We should check whether any candidates may become ready after this
  // because there we will check whether the candidate is generated by the ready
  // ports, which may include this port.
  bool pruned = false;
  if (CandidatePairable(c, port) && !data->has_pairable_candidate()) {
    data->set_has_pairable_candidate(true);

    if (port->Type() == RELAY_PORT_TYPE) {
      if (turn_port_prune_policy_ == webrtc::KEEP_FIRST_READY) {
        pruned = PruneNewlyPairableTurnPort(data);
      } else if (turn_port_prune_policy_ == webrtc::PRUNE_BASED_ON_PRIORITY) {
        pruned = PruneTurnPorts(port);
      }
    }

    // If the current port is not pruned yet, SignalPortReady.
    if (!data->pruned()) {
      RTC_LOG(LS_INFO) << port->ToString() << ": Port ready.";
      SignalPortReady(this, port);
      port->KeepAliveUntilPruned();
    }
  }

  if (data->ready() && CheckCandidateFilter(c)) {
    std::vector<Candidate> candidates;
    candidates.push_back(allocator_->SanitizeCandidate(c));
    SignalCandidatesReady(this, candidates);
  } else {
    RTC_LOG(LS_INFO) << "Discarding candidate because it doesn't match filter.";
  }

  // If we have pruned any port, maybe need to signal port allocation done.
  if (pruned) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::OnCandidateError(
    Port* port,
    const IceCandidateErrorEvent& event) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(FindPort(port));
  if (event.address.empty()) {
    candidate_error_events_.push_back(event);
  } else {
    SignalCandidateError(this, event);
  }
}

Port* BasicPortAllocatorSession::GetBestTurnPortForNetwork(
    const std::string& network_name) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  Port* best_turn_port = nullptr;
  for (const PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && data.ready() &&
        (!best_turn_port || ComparePort(data.port(), best_turn_port) > 0)) {
      best_turn_port = data.port();
    }
  }
  return best_turn_port;
}

bool BasicPortAllocatorSession::PruneNewlyPairableTurnPort(
    PortData* newly_pairable_port_data) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(newly_pairable_port_data->port()->Type() == RELAY_PORT_TYPE);
  // If an existing turn port is ready on the same network, prune the newly
  // pairable port.
  const std::string& network_name =
      newly_pairable_port_data->port()->Network()->name();

  for (PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && data.ready() &&
        &data != newly_pairable_port_data) {
      RTC_LOG(LS_INFO) << "Port pruned: "
                       << newly_pairable_port_data->port()->ToString();
      newly_pairable_port_data->Prune();
      return true;
    }
  }
  return false;
}

bool BasicPortAllocatorSession::PruneTurnPorts(Port* newly_pairable_turn_port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  // Note: We determine the same network based only on their network names. So
  // if an IPv4 address and an IPv6 address have the same network name, they
  // are considered the same network here.
  const std::string& network_name = newly_pairable_turn_port->Network()->name();
  Port* best_turn_port = GetBestTurnPortForNetwork(network_name);
  // |port| is already in the list of ports, so the best port cannot be nullptr.
  RTC_CHECK(best_turn_port != nullptr);

  bool pruned = false;
  std::vector<PortData*> ports_to_prune;
  for (PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && !data.pruned() &&
        ComparePort(data.port(), best_turn_port) < 0) {
      pruned = true;
      if (data.port() != newly_pairable_turn_port) {
        // These ports will be pruned in PrunePortsAndRemoveCandidates.
        ports_to_prune.push_back(&data);
      } else {
        data.Prune();
      }
    }
  }

  if (!ports_to_prune.empty()) {
    RTC_LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                     << " low-priority TURN ports";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }
  return pruned;
}

void BasicPortAllocatorSession::PruneAllPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (PortData& data : ports_) {
    data.Prune();
  }
}

void BasicPortAllocatorSession::OnPortComplete(Port* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << port->ToString()
                   << ": Port completed gathering candidates.";
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);

  // Ignore any late signals.
  if (!data->inprogress()) {
    return;
  }

  // Moving to COMPLETE state.
  data->set_state(PortData::STATE_COMPLETE);
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnPortError(Port* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << port->ToString()
                   << ": Port encountered error while gathering candidates.";
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);
  // We might have already given up on this port and stopped it.
  if (!data->inprogress()) {
    return;
  }

  // SignalAddressError is currently sent from StunPort/TurnPort.
  // But this signal itself is generic.
  data->set_state(PortData::STATE_ERROR);
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

bool BasicPortAllocatorSession::CheckCandidateFilter(const Candidate& c) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  return IsAllowedByCandidateFilter(c, candidate_filter_);
}

bool BasicPortAllocatorSession::CandidatePairable(const Candidate& c,
                                                  const Port* port) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  bool candidate_signalable = CheckCandidateFilter(c);

  // When device enumeration is disabled (to prevent non-default IP addresses
  // from leaking), we ping from some local candidates even though we don't
  // signal them. However, if host candidates are also disabled (for example, to
  // prevent even default IP addresses from leaking), we still don't want to
  // ping from them, even if device enumeration is disabled.  Thus, we check for
  // both device enumeration and host candidates being disabled.
  bool network_enumeration_disabled = c.address().IsAnyIP();
  bool can_ping_from_candidate =
      (port->SharedSocket() || c.protocol() == TCP_PROTOCOL_NAME);
  bool host_candidates_disabled = !(candidate_filter_ & CF_HOST);

  return candidate_signalable ||
         (network_enumeration_disabled && can_ping_from_candidate &&
          !host_candidates_disabled);
}

void BasicPortAllocatorSession::OnPortAllocationComplete(
    AllocationSequence* seq) {
  RTC_DCHECK_RUN_ON(network_thread_);
  // Send candidate allocation complete signal if all ports are done.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::MaybeSignalCandidatesAllocationDone() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (CandidatesAllocationDone()) {
    if (pooled()) {
      RTC_LOG(LS_INFO) << "All candidates gathered for pooled session.";
    } else {
      RTC_LOG(LS_INFO) << "All candidates gathered for " << content_name()
                       << ":" << component() << ":" << generation();
    }
    for (const auto& event : candidate_error_events_) {
      SignalCandidateError(this, event);
    }
    candidate_error_events_.clear();
    SignalCandidatesAllocationDone(this);
  }
}

void BasicPortAllocatorSession::OnPortDestroyed(PortInterface* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (std::vector<PortData>::iterator iter = ports_.begin();
       iter != ports_.end(); ++iter) {
    if (port == iter->port()) {
      ports_.erase(iter);
      RTC_LOG(LS_INFO) << port->ToString() << ": Removed port from allocator ("
                       << static_cast<int>(ports_.size()) << " remaining)";
      return;
    }
  }
  RTC_NOTREACHED();
}

BasicPortAllocatorSession::PortData* BasicPortAllocatorSession::FindPort(
    Port* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (std::vector<PortData>::iterator it = ports_.begin(); it != ports_.end();
       ++it) {
    if (it->port() == port) {
      return &*it;
    }
  }
  return NULL;
}

std::vector<BasicPortAllocatorSession::PortData*>
BasicPortAllocatorSession::GetUnprunedPorts(
    const std::vector<rtc::Network*>& networks) {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<PortData*> unpruned_ports;
  for (PortData& port : ports_) {
    if (!port.pruned() &&
        absl::c_linear_search(networks, port.sequence()->network())) {
      unpruned_ports.push_back(&port);
    }
  }
  return unpruned_ports;
}

void BasicPortAllocatorSession::PrunePortsAndRemoveCandidates(
    const std::vector<PortData*>& port_data_list) {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<PortInterface*> pruned_ports;
  std::vector<Candidate> removed_candidates;
  for (PortData* data : port_data_list) {
    // Prune the port so that it may be destroyed.
    data->Prune();
    pruned_ports.push_back(data->port());
    if (data->has_pairable_candidate()) {
      GetCandidatesFromPort(*data, &removed_candidates);
      // Mark the port as having no pairable candidates so that its candidates
      // won't be removed multiple times.
      data->set_has_pairable_candidate(false);
    }
  }
  if (!pruned_ports.empty()) {
    SignalPortsPruned(this, pruned_ports);
  }
  if (!removed_candidates.empty()) {
    RTC_LOG(LS_INFO) << "Removed " << removed_candidates.size()
                     << " candidates";
    SignalCandidatesRemoved(this, removed_candidates);
  }
}
}  // namespace
}  // namespace cricket
