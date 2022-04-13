#include "net/dcsctp/speed_test/datachannel.h"

#include <iostream>
#include <thread>

#include "api/create_peerconnection_factory.h"
#include "net/dcsctp/speed_test/common.h"
#include "net/dcsctp/speed_test/peerconnection.h"
#include "net/dcsctp/speed_test/signaling/common.h"

const uint64_t kMeasurementIntervalMs = 1000;

DataChannel::DataChannel(
    Peerconnection& parent,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : parent(parent),
      _last(GetTimeMillis()),
      _total(0),
      _data_channel(data_channel),
      _data_thread_done(false) {
  _can_send.store(true);
  _data_channel->RegisterObserver(this);
}

void DataChannel::OnStateChange() {
  std::cout << "#-> DataChannel::StateChange " << _data_channel->label() << " "
            << std::endl;

  if (_data_channel->state() == webrtc::DataChannelInterface::kOpen) {
    if (parent._offerer) {
      std::cout << "    Peerconnection::" << __func__
                << " ################### START SENDER  "
                << _data_channel->label() << " #################" << std::endl;
      _datachannel_thread.reset(
          new std::thread(&DataChannel::SenderThread, this));
    }
  }
  std::cout << "<-# DataChannel::StateChange " << _data_channel->label() << " "
            << std::endl;
}

void DataChannel::OnMessage(const webrtc::DataBuffer& buffer) {
  // const uint8_t *b = buffer.data.data<uint8_t>();
  uint64_t now = GetTimeMillis();
  _total += buffer.data.size();

  if ((now - _last) > kMeasurementIntervalMs) {
    int64_t speed = (_total << 3) / (((now - _last) / 1000) * (1024 * 1024));
    std::cout << speed << " Mbps"
              << "  " << _total << " bytes [" << parent._id << "] "
              << _data_channel->label() << "  " << std::endl;
    _last = now;
    _total = 0;
  }
}

void DataChannel::OnBufferedAmountChange(uint64_t previous_amount) {
  bool prev = _can_send.load();
  bool curr = false;
  if (prev) {
    curr = (_data_channel->buffered_amount() < gDataChannelBufferHighSize);
  } else {
    curr = (_data_channel->buffered_amount() < gDataChannelBufferLowSize);
  }

  if (prev != curr) {
    _can_send.store(curr);
    if (curr)
      _cond.notify_one();
  }
}

void DataChannel::SenderThread(void) {
  rtc::CopyOnWriteBuffer cb(gDataChannelChunkSize);
  std::memset((void*)cb.data(), 0, cb.size());
  webrtc::DataBuffer buffer(cb, true);

  while (!_data_thread_done) {
    if (_can_send.load()) {
      _data_channel->Send(buffer);
    } else {
      std::unique_lock<std::mutex> lock(_mutex);
      _cond.wait(lock);
    }
  }
}

void DataChannel::Close() {
  _data_thread_done = true;
  if (_datachannel_thread) {
    _datachannel_thread->join();
    _datachannel_thread.reset();
  }
}
