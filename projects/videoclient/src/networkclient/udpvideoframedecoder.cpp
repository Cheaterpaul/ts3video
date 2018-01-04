#include "udpvideoframedecoder.h"

#include "videolib/vp8frame.h"
#include "videolib/timeutil.h"

///////////////////////////////////////////////////////////////////////////////
// Helper
///////////////////////////////////////////////////////////////////////////////

class VideoFrameLessComparison
{
public:
	VideoFrameLessComparison(bool reverse = false)
		: _reverse(reverse)
	{
	}
	bool operator()(const VP8Frame& l, const VP8Frame& r)
	{
		if (_reverse)
			return (l.time > r.time);
		return (l.time < r.time);
	}
	bool operator()(const VP8Frame* l, const VP8Frame* r)
	{
		return operator()(*l, *r);
	}
private:
	bool _reverse;
};

template <class M>
void map_remove_last_element(M& map)
{
	auto it = map.begin();
	std::advance(it, map.size() - 1);
	map.erase(it);
}

///////////////////////////////////////////////////////////////////////////////
// VideoFrameUdpDecoder
///////////////////////////////////////////////////////////////////////////////

VideoFrameUdpDecoder::VideoFrameUdpDecoder() :
	_last_error(VideoFrameUdpDecoder::NoError),
	_maximum_distinct_frames(16),
	_frame_buffers(),
	_complete_frames_queue(),
	_last_completed_frame_id(0),
	_wait_for_frame_type(VP8Frame::NORMAL),
	_received_key_frame_count(0)
{
}

VideoFrameUdpDecoder::~VideoFrameUdpDecoder()
{
	checkFrameBuffers(0);
	checkCompleteFramesQueue(0);
}

int VideoFrameUdpDecoder::add(UDP::VideoFrameDatagram* dpart)
{
	if (!dpart)
	{
		_last_error = VideoFrameUdpDecoder::InvalidParameter;
		return _last_error;
	}

	// Comment out for (ts3video/#54) - NOT YET TESTED ENOUGH!
	// Skip datagrams of frames, which has already been completed.
	if (dpart->frameId <= _last_completed_frame_id && (_last_completed_frame_id - dpart->frameId) < 256)
	{
		delete dpart;
		dpart = nullptr;
		_last_error = VideoFrameUdpDecoder::AlreadyProcessed;
		return _last_error;
	}

	// Get existing frame buffer or create one.
	auto found_i = _frame_buffers.find(dpart->frameId);
	if (found_i == _frame_buffers.end())
	{
		_frame_buffers[dpart->frameId] = new DGPtrListMapValue();
		found_i = _frame_buffers.find(dpart->frameId);
		(*found_i).second->datagrams = DGPtrList(dpart->count);
		(*found_i).second->first_received_datagram_time = get_local_timestamp();
	}
	auto& buffer = (*found_i).second->datagrams;

	// Due to FEC (forward error correction) or recovery,
	// it is possible that the same datagram occurs multiple times.
	if (buffer[dpart->index] != nullptr)
	{
		delete dpart;
		dpart = nullptr;
		_last_error = VideoFrameUdpDecoder::AlreadyProcessed;
		return _last_error;
	}

	// Add datagram to buffer.
	buffer[dpart->index] = dpart;

	// Do nothing more as long as the buffer is not complete.
	if (!isComplete(buffer))
	{
		checkFrameBuffers(_maximum_distinct_frames);
		_last_error = VideoFrameUdpDecoder::NoError;
		return _last_error;
	}

	// At this point the "buffer" is complete and contains an entire video-frame.
	// Create VideoFrame object from buffer.
	auto frame = createFrame(buffer);
	_complete_frames_queue[frame->time] = frame;

	//removeFromFrameBuffer(dpart->timestamp);
	checkFrameBuffers(_maximum_distinct_frames);
	checkCompleteFramesQueue(_maximum_distinct_frames);

	_last_error = VideoFrameUdpDecoder::NoError;
	return _last_error;
}

VP8Frame* VideoFrameUdpDecoder::next()
{
	auto i_begin = _complete_frames_queue.begin();
	if (i_begin == _complete_frames_queue.end())
	{
		// No finished frames in queue.
		// Wait for new frames (TODO do not wait longer than X ms).
		// Check frame buffers for next frame, and whether we it requires a resend.
		// ...
		return 0;
	}

	auto frame_id = (*i_begin).first;
	auto frame    = (*i_begin).second;

	if (_received_key_frame_count == 0 && frame->type != VP8Frame::KEY)
	{
		// As long as no key-frame has been received,
		// all other received frames are useless.
		_complete_frames_queue.erase(i_begin);
		_wait_for_frame_type = VP8Frame::KEY;
		delete frame;
		return nullptr;
	}
	else if (frame_id <= _last_completed_frame_id && (_last_completed_frame_id - frame_id) < 256)
	{
		_complete_frames_queue.erase(i_begin);
		delete frame;
		return nullptr;
	}
	else if (frame->type == VP8Frame::KEY)
	{
		_complete_frames_queue.erase(i_begin);
		_last_completed_frame_id = frame_id;
		_wait_for_frame_type = VP8Frame::NORMAL;
		++_received_key_frame_count;
		return frame;
	}
	else if (_wait_for_frame_type != VP8Frame::NORMAL)
	{
		if (frame->type == _wait_for_frame_type)
		{
			_complete_frames_queue.erase(i_begin);
			_last_completed_frame_id = frame_id;
			_wait_for_frame_type = VP8Frame::NORMAL;
			return frame;
		}
		else
		{
			_complete_frames_queue.erase(i_begin);
			_last_completed_frame_id = frame_id;
			delete frame;
			return nullptr;
		}
	}
	else if (frame_id != (_last_completed_frame_id + 1))
	{
		// The next frame in queue is not the correct next frame.
		_wait_for_frame_type = VP8Frame::KEY;
		_complete_frames_queue.erase(i_begin);
		_last_completed_frame_id = frame_id;
		return frame;
	}
	else
	{
		// Just a normal next frame.
		_complete_frames_queue.erase(i_begin);
		_last_completed_frame_id = frame_id;
		_wait_for_frame_type = VP8Frame::NORMAL;
		return frame;
	}
}

int VideoFrameUdpDecoder::getWaitsForType() const
{
	return _wait_for_frame_type;
}

bool VideoFrameUdpDecoder::isComplete(const DGPtrList& buffer)
{
	auto i_end = buffer.end();
	for (auto i = buffer.begin(); i != i_end; ++i)
	{
		if (*i == nullptr)
			return false;
	}
	return true;
}

VP8Frame* VideoFrameUdpDecoder::createFrame(const DGPtrList& buffer) const
{
	QByteArray data;
	for (auto i = buffer.begin(), i_end = buffer.end(); i != i_end; ++i)
	{
		auto dgvideo = *i;
		data.append(QByteArray((const char*)dgvideo->data, dgvideo->size));
	}

	auto frame = new VP8Frame();
	QDataStream in(data);
	in >> *frame;
	return frame;
}

void VideoFrameUdpDecoder::checkFrameBuffers(unsigned int max_size)
{
	// Check number of buffers, it may not be greater than "_maximum_distinct_frames".
	// Delete buffers from map until the maximum number of buffers has been reached.
	while (_frame_buffers.size() > max_size)
	{
		auto val = (*_frame_buffers.begin()).second;
		auto& fb = (*_frame_buffers.begin()).second->datagrams;
		while (!fb.empty())
		{
			auto dg = fb.back();
			if (dg)
				delete dg;
			fb.pop_back();
		}
		delete val;
		_frame_buffers.erase(_frame_buffers.begin());
	}
}

void VideoFrameUdpDecoder::checkCompleteFramesQueue(unsigned int max_size)
{
	while (_complete_frames_queue.size() > max_size)
	{
		auto video_frame = (*_complete_frames_queue.begin()).second;
		delete video_frame;
		_complete_frames_queue.erase(_complete_frames_queue.begin());
	}
}

void VideoFrameUdpDecoder::removeFromFrameBuffer(unsigned long long ts)
{
	auto found_i = _frame_buffers.find(ts);
	if (found_i == _frame_buffers.end())
	{
		return;
	}
	auto& fb = found_i->second->datagrams;
	while (!fb.empty())
	{
		auto dg = fb.back();
		if (dg)
			delete dg;
		fb.pop_back();
	}
	delete found_i->second;
	_frame_buffers.erase(found_i);
}