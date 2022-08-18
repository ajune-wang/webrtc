#pragma once
#include <condition_variable>
#include <mutex>
#include <thread>
#include "api/create_peerconnection_factory.h"
class Peerconnection;

class DataChannel: public webrtc::DataChannelObserver {
public:
	DataChannel(Peerconnection* parent, rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
	~DataChannel();
	void Close();
	void setDataChannel(std::shared_ptr<DataChannel> dc);
private:
	void OnStateChange() override;
	void OnMessage(const webrtc::DataBuffer &buffer) override;
	void OnBufferedAmountChange(uint64_t previous_amount) override;
	void SenderThread();
private:
	Peerconnection* _parent;
	uint64_t _last;
	uint64_t _total;
	rtc::scoped_refptr<webrtc::DataChannelInterface> _data_channel;
	std::shared_ptr<std::thread> _datachannel_thread;
	std::atomic<bool> _data_thread_done;
	std::mutex _mutex;
	std::condition_variable _cond;
	std::atomic<bool> _can_send;
	uint64_t _lifetimeMs;
	uint8_t _content;
	std::string _label;
	std::shared_ptr<DataChannel> _dc;
};
