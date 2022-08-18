#include <iostream>
#include <thread>
#include <random>
#include <algorithm>
#include <iomanip>

#include "api/create_peerconnection_factory.h"

#include "common.h"
#include "datachannel.h"
#include "peerconnection.h"
#include "rtc_tools/data_channel_stresstest/signaling/common.h"


static void dump(const void *void_data, size_t offset, unsigned int size) {
	enum {
		bytes_per_line = 16
	};
	const unsigned char *data = (const unsigned char*) (void_data) + offset;
	const unsigned char *data_end = data + size;

	char hex_dump[bytes_per_line * 3 + 1];
	char ascii_dump[bytes_per_line + 1];

	char *const end_hex_dump = hex_dump + sizeof(hex_dump) - 1;
	char *const end_ascii_dump = ascii_dump + sizeof(ascii_dump) - 1;

	*end_hex_dump = 0;
	*end_ascii_dump = 0;

	for (int line_bytes = offset; data < data_end; line_bytes += bytes_per_line) {
		int row_len = data_end - data;

		char *curr_hex_dump = hex_dump;
		char *curr_ascii_dump = ascii_dump;

		if (row_len > bytes_per_line) {
			row_len = bytes_per_line;
		}

		for (int pos = 0; pos < row_len; ++pos) {
			unsigned int val = *data++;

			sprintf(curr_hex_dump, "%02X ", val);
			curr_hex_dump += 3;
			val &= 0x7F;

			if ((val < 0x20) || (val == 0x7F)) {
				val = '.';
			}

			*curr_ascii_dump++ = val;
		}

		if (row_len < bytes_per_line) {
			memset(curr_hex_dump, 0x20, (end_hex_dump - curr_hex_dump));
			memset(curr_ascii_dump, 0x20, (end_ascii_dump - curr_ascii_dump));
		}

		std::cout << format("%03X| %s %s", line_bytes, hex_dump, ascii_dump) << std::endl;
	}
}

static uint8_t hexchar_to_dec(char c) {
	return c >= 'A' ? (c - 'A' + 10) : (c - '0');
}

DataChannel::DataChannel(Peerconnection* parent, rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) :
		_parent(parent), _last(GetTimeMillis()), _total(0), _data_channel(data_channel), _data_thread_done(false) {
	_can_send = true;
	_label = _data_channel->label();
	_content = hexchar_to_dec(_label[_label.length() - 2]) * 16 + hexchar_to_dec(_label[_label.length() - 1]);
	_data_channel->RegisterObserver(this);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dist(500, 1500);
	_lifetimeMs = dist(gen);
	std::cout << "    DataChannel::DataChannel() label=" << _label << " _lifetimeMs=" << _lifetimeMs << " " << std::endl;
}

DataChannel::~DataChannel() {
	std::cout << "    DataChannel::~DataChannel() label=" << _label << " " << std::endl;
}

void DataChannel::setDataChannel(std::shared_ptr<DataChannel> dc) {
	_dc = dc;
}

void DataChannel::OnStateChange() {
	std::cout << "#-> DataChannel::StateChange " << _label << " " << std::endl;
	auto state = _data_channel->state();
	if (state == webrtc::DataChannelInterface::kOpen) {
		if (!_parent->_offerer) {
			std::cout << "    DataChannel::" << __func__ << " ################### START SENDER label=" << _label << " state=" << state << "#################"
					<< std::endl;
			_datachannel_thread.reset(new std::thread(&DataChannel::SenderThread, this));
			_datachannel_thread->detach();
		}
	} else if (state == webrtc::DataChannelInterface::kClosing) {
		_data_thread_done = true;
		_cond.notify_one();
	} else if (state == webrtc::DataChannelInterface::kClosed) {
		Close();
	}
	std::cout << "<-# DataChannel::StateChange label=" << _label << " state=" << state << " " << std::endl;
}

void DataChannel::OnMessage(const webrtc::DataBuffer &buffer) {

	uint64_t now = GetTimeMillis();
	_total += buffer.data.size();
	if (buffer.data.size() > 0) {
		const uint8_t *data = buffer.data.data();
		for (size_t i = 0; i < buffer.data.size(); ++i) {
			if (data[i] != _content) {
				std::cout << "    DataChannel::" << __func__ << " ################### DATA MISMATCH " << _label << " (id: " << _data_channel->id() << ")"
						<< " EXPECTED: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int) _content
						<< " RECEIVED: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int) data[i] << " #################" << std::endl;
				dump(data, (i & 0x100), std::min(buffer.data.size(), (size_t)256));
				std::cout << "    DataChannel::" << __func__ << " ################### ABORTING #################"<< std::endl;
				abort();
			}
		}
	}

	if ((now - _last) > _lifetimeMs) {
		_last = now;
		Close();
	}

}

void DataChannel::OnBufferedAmountChange(uint64_t previous_amount) {

	std::unique_lock < std::mutex > lock(_mutex);
	bool curr = false;
	if (_can_send) {
		curr = (_data_channel->buffered_amount() < gDataChannelBufferHighSize);
	} else {
		curr = (_data_channel->buffered_amount() < gDataChannelBufferLowSize);
	}

	if (_can_send != curr) {
		_can_send = curr;
		if (curr)
			_cond.notify_one();
	}
}

void DataChannel::SenderThread(void) {

	std::cout << "#-> DataChannel::" << __func__ << " label=" << _label << " " << std::endl;

	rtc::CopyOnWriteBuffer cb(gDataChannelChunkSize);
	std::memset((void*) cb.data(), _content, cb.size());
	webrtc::DataBuffer buffer(cb, true);

	while (!_data_thread_done) {
		{
			std::unique_lock < std::mutex > lock(_mutex);
			if (!_can_send) {
				_cond.wait(lock);
				if (_data_thread_done) {
					break;
				}
			}
		}
		if (_data_channel) {
			std::cout << "    DataChannel::" << __func__ << " Sending message on channel label=" << _label << " " << std::endl;
			_data_channel->Send(buffer);
		}
	}

	std::cout << "    DataChannel::" << __func__ << " Closing channel label=" << _label << " " << std::endl;
	_data_channel->UnregisterObserver();
	_data_channel->Close();
	_parent->DeleteDataChannel(_label);
	_dc = NULL;
	std::cout << "<-# DataChannel::" << __func__ << " label=" << _label << " " << std::endl;
}

void DataChannel::Close() {

	std::cout << "#-> DataChannel::" << __func__ << " " << _label << " " << std::endl;
	Peerconnection* parent = _parent;
	if (parent->_offerer) {
		_data_channel->UnregisterObserver();
		_data_channel->Close();
		parent->DeleteDataChannel(_label);
		_dc = NULL;

		// Lets create new channel
		parent->CreateDataChannel();
	} else {
		// Answerer will close channel from sender thread
	}

	std::cout << "<-# DataChannel::" << __func__ << " " << _label << " " << std::endl;
}
