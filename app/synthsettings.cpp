#include "synthsettings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace g1app
{
	namespace
	{
		constexpr uint64_t BootMs = 2500;		// the OS is up and has looked at its flash
		constexpr uint64_t QuietMs = 500;		// the editor's own exchanges come closer together
		constexpr uint64_t ReplyMs = 1500;
		constexpr uint64_t WriteMs = 300;		// the OS takes them without a word; then they are read back
		constexpr uint64_t FilterTailMs = 300;	// late replies to an abandoned request

		constexpr uint8_t CcIAm = 0x00, CcAck = 0x16, CcPatch = 0x17;
		constexpr uint8_t SectionType = 3;
		constexpr uint8_t ChannelMarker = 27;	// after each slot's channel, as the OS writes it

		std::vector<uint8_t> frame(const uint8_t _cc, const uint8_t _slot, const std::vector<uint8_t>& _payload)
		{
			std::vector<uint8_t> m = {0xf0, 0x33, static_cast<uint8_t>(((_cc & 0x1f) << 2) | (_slot & 3)), 0x06};
			m.insert(m.end(), _payload.begin(), _payload.end());
			uint32_t sum = 0;
			for(const auto b : m)
				sum += b;
			m.push_back(static_cast<uint8_t>(sum & 0x7f));
			m.push_back(0xf7);
			return m;
		}

		std::vector<uint8_t> pack7(const std::vector<uint8_t>& _raw)
		{
			std::vector<uint8_t> out;
			uint32_t buffer = 0;
			int held = 0;
			for(const auto b : _raw)
			{
				buffer = (buffer << 8) | b;
				held += 8;
				while(held >= 7)
				{
					held -= 7;
					out.push_back(static_cast<uint8_t>((buffer >> held) & 0x7f));
				}
			}
			if(held > 0)
				out.push_back(static_cast<uint8_t>((buffer << (7 - held)) & 0x7f));
			return out;
		}

		std::vector<uint8_t> unpack7(const std::vector<uint8_t>& _in)
		{
			std::vector<uint8_t> out;
			uint32_t buffer = 0;
			int held = 0;
			for(const auto c : _in)
			{
				buffer = (buffer << 7) | (c & 0x7f);
				held += 7;
				if(held >= 8)
				{
					held -= 8;
					out.push_back(static_cast<uint8_t>((buffer >> held) & 0xff));
				}
			}
			return out;
		}

		// Whole SysEx messages out of a byte stream; what is left over stays in _buf.
		template<typename F> void forEachMessage(std::vector<uint8_t>& _buf, F&& _f)
		{
			size_t pos = 0;
			for(;;)
			{
				const auto start = std::find(_buf.begin() + static_cast<long>(pos), _buf.end(), uint8_t(0xf0));
				if(start == _buf.end())
				{
					pos = _buf.size();
					break;
				}
				const auto end = std::find(start, _buf.end(), uint8_t(0xf7));
				if(end == _buf.end())
				{
					pos = static_cast<size_t>(start - _buf.begin());
					break;
				}
				_f(std::vector<uint8_t>(start, end + 1));
				pos = static_cast<size_t>(end - _buf.begin()) + 1;
			}
			_buf.erase(_buf.begin(), _buf.begin() + static_cast<long>(pos));
		}

		// G1_SETTINGS_TRACE=1: every message the link sends and every one the G1 sends, on stderr.
		void traceMsg(const char* _dir, const uint64_t _nowMs, const std::vector<uint8_t>& _m)
		{
			static const bool on = std::getenv("G1_SETTINGS_TRACE") != nullptr;
			if(!on)
				return;
			std::fprintf(stderr, "[settings %6llu ms] %s", static_cast<unsigned long long>(_nowMs), _dir);
			for(const auto b : _m)
				std::fprintf(stderr, " %02x", b);
			std::fprintf(stderr, "\n");
		}

		bool isClavia(const std::vector<uint8_t>& _m) { return _m.size() >= 6 && _m[1] == 0x33; }
		uint8_t ccOf(const std::vector<uint8_t>& _m) { return static_cast<uint8_t>(_m[2] >> 2); }
		bool isPacket(const uint8_t _cc) { return _cc >= 0x1c && _cc <= 0x1f; }
	}

	// ____________________________________________________________________________________________
	// The section: type 3, then clock source:1 vel min:7, LEDs:1 vel max:7, bpm:8, local:1
	// keyboard mode:1 pedal:1 global sync:5, master tune:8, PC receive:1 PC send:1 knob mode:1 0:5,
	// the name (up to 16 characters, ended by a zero when shorter), and per slot 0:3 channel:5 27:8.

	std::vector<uint8_t> SynthSettings::encode() const
	{
		const auto bit = [](const bool _b) { return static_cast<uint8_t>(_b ? 1 : 0); };
		std::vector<uint8_t> s = {SectionType};
		s.push_back(static_cast<uint8_t>((bit(clockInternal) << 7) | (velScaleMin & 0x7f)));
		s.push_back(static_cast<uint8_t>((bit(ledsActive) << 7) | (velScaleMax & 0x7f)));
		s.push_back(static_cast<uint8_t>(clockBpm & 0xff));
		s.push_back(static_cast<uint8_t>((bit(localOn) << 7) | ((keyboardMode & 1) << 6) | ((pedalPolarity & 1) << 5) | (globalSync & 0x1f)));
		s.push_back(static_cast<uint8_t>(std::clamp(masterTune, -127, 127) & 0xff));
		s.push_back(static_cast<uint8_t>((bit(programChangeReceive) << 7) | (bit(programChangeSend) << 6) | ((knobMode & 1) << 5)));
		const auto n = std::min<size_t>(name.size(), 16);
		for(size_t i = 0; i < n; ++i)
			s.push_back(static_cast<uint8_t>(name[i]));
		if(n < 16)
			s.push_back(0);
		for(const auto c : midiChannel)
		{
			s.push_back(static_cast<uint8_t>(c & 0x1f));
			s.push_back(ChannelMarker);
		}
		return s;
	}

	bool SynthSettings::decode(const std::vector<uint8_t>& _s, SynthSettings& _out)
	{
		if(_s.size() < 8 || _s[0] != SectionType)
			return false;
		SynthSettings r;
		r.clockInternal = (_s[1] & 0x80) != 0;
		r.velScaleMin = _s[1] & 0x7f;
		r.ledsActive = (_s[2] & 0x80) != 0;
		r.velScaleMax = _s[2] & 0x7f;
		r.clockBpm = _s[3];
		r.localOn = (_s[4] & 0x80) != 0;
		r.keyboardMode = (_s[4] >> 6) & 1;
		r.pedalPolarity = (_s[4] >> 5) & 1;
		r.globalSync = _s[4] & 0x1f;
		r.masterTune = static_cast<int8_t>(_s[5]);
		r.programChangeReceive = (_s[6] & 0x80) != 0;
		r.programChangeSend = (_s[6] & 0x40) != 0;
		r.knobMode = (_s[6] >> 5) & 1;
		size_t p = 7;
		r.name.clear();
		while(p < _s.size() && r.name.size() < 16 && _s[p] != 0)
			r.name += static_cast<char>(_s[p++]);
		if(r.name.size() < 16)
			++p;	// the zero
		for(auto& c : r.midiChannel)
		{
			if(p + 2 > _s.size() || (_s[p] & 0xe0) != 0)
				return false;
			c = _s[p] & 0x1f;
			p += 2;
		}
		_out = r;
		return true;
	}

	// ____________________________________________________________________________________________
	// Any thread

	void SynthSettingsLink::read()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_readWanted = true;
	}

	void SynthSettingsLink::write(const SynthSettings& _settings)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_toWrite = _settings;
		m_writeWanted = true;
	}

	bool SynthSettingsLink::settings(SynthSettings& _out, uint64_t& _revision) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		_out = m_settings;
		_revision = m_revision;
		return m_revision > 0;
	}

	void SynthSettingsLink::reset()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_readWanted = true;
		m_writeWanted = false;
		m_state = State::Idle;
		m_greeted = false;
		m_pid = 0;
		m_deadline = m_filterUntil = m_lastActivity = 0;
		m_packets.clear();
		m_rx.clear();
		m_editorRx.clear();
	}

	// ____________________________________________________________________________________________
	// Worker thread

	void SynthSettingsLink::send(const std::vector<uint8_t>& _msg, const uint64_t _nowMs, std::vector<uint8_t>& _toG1)
	{
		traceMsg("->", _nowMs, _msg);
		_toG1.insert(_toG1.end(), _msg.begin(), _msg.end());
		m_filterUntil = UINT64_MAX;	// until the transaction ends, then a tail
	}

	void SynthSettingsLink::finish(const uint64_t _nowMs)
	{
		m_state = State::Idle;
		m_filterUntil = _nowMs + FilterTailMs;
		m_packets.clear();
	}

	void SynthSettingsLink::editorSent(const std::vector<uint8_t>& _bytes, const uint64_t _nowMs)
	{
		m_editorRx.insert(m_editorRx.end(), _bytes.begin(), _bytes.end());
		forEachMessage(m_editorRx, [&](const std::vector<uint8_t>&)
		{
			m_lastActivity = _nowMs;
			// The editor talks: the link gets out of the way and tries again once it is quiet.
			if(m_state == State::Reading)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_readWanted = true;
				finish(_nowMs);
			}
		});
	}

	void SynthSettingsLink::g1Sent(const std::vector<uint8_t>& _bytes, const uint64_t _nowMs, std::vector<uint8_t>& _toEditor)
	{
		m_rx.insert(m_rx.end(), _bytes.begin(), _bytes.end());
		forEachMessage(m_rx, [&](const std::vector<uint8_t>& _m)
		{
			const bool filtering = m_state != State::Idle || _nowMs < m_filterUntil;
			bool hide = false;
			if(isClavia(_m))
			{
				const auto cc = ccOf(_m);
				if(filtering && (isPacket(cc) || cc == CcAck))
					hide = true;
				if(cc == CcIAm && m_state == State::Greeting)
					hide = true;
				if(hide)
					traceMsg("<-", _nowMs, _m);

				if(m_state == State::Greeting && cc == CcIAm && _m.size() > 4 && _m[4] == 0x01)
					finish(_nowMs);		// m_readWanted is still set: tick() asks again
				else if(m_state == State::Reading && isPacket(cc) && _m.size() >= 7)
				{
					if(cc & 1)
						m_packets.clear();
					m_pid = static_cast<uint8_t>(_m[4] & 0x3f);
					m_packets.insert(m_packets.end(), _m.begin() + 5, _m.end() - 2);	// after cmd/pid, before checksum
					if(cc & 2)
					{
						SynthSettings s;
						if(SynthSettings::decode(unpack7(m_packets), s))
						{
							std::lock_guard<std::mutex> lock(m_mutex);
							m_settings = s;
							++m_revision;
							finish(_nowMs);
						}
						m_packets.clear();
					}
				}
			}
			if(!hide)
				_toEditor.insert(_toEditor.end(), _m.begin(), _m.end());
		});
	}

	void SynthSettingsLink::tick(const uint64_t _nowMs, std::vector<uint8_t>& _toG1)
	{
		switch(m_state)
		{
		case State::Idle:
		{
			if(_nowMs < BootMs || _nowMs < m_filterUntil || _nowMs < m_lastActivity + QuietMs)
				break;
			std::lock_guard<std::mutex> lock(m_mutex);
			if(m_writeWanted)
			{
				m_writeWanted = false;
				auto payload = pack7(m_toWrite.encode());
				payload.insert(payload.begin(), m_pid);
				send(frame(0x1f, 0, payload), _nowMs, _toG1);
				m_state = State::Writing;
				m_deadline = _nowMs + WriteMs;
				m_readWanted = true;	// then read back what the OS made of them
			}
			else if(m_readWanted)
			{
				m_readWanted = false;
				send(frame(CcPatch, 0, {0x44, 0x02, 0x06, 0x08, 0x04}), _nowMs, _toG1);	// RequestSynthSettings, as NME
				m_state = State::Reading;
				m_deadline = _nowMs + ReplyMs;
			}
			break;
		}
		case State::Reading:
			if(_nowMs >= m_deadline)
			{
				finish(_nowMs);
				// No answer: the OS may not have met an editor yet. Greet it once, as one would.
				if(!m_greeted)
				{
					m_greeted = true;
					std::lock_guard<std::mutex> lock(m_mutex);
					m_readWanted = true;
					send({0xf0, 0x33, 0x00, 0x06, 0x00, 0x03, 0x03, 0xf7}, _nowMs, _toG1);	// IAm, as an editor
					m_state = State::Greeting;
					m_deadline = _nowMs + ReplyMs;
				}
			}
			break;
		case State::Greeting:
		case State::Writing:
			if(_nowMs >= m_deadline)
				finish(_nowMs);
			break;
		}
	}
}
