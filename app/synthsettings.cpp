#include "synthsettings.h"

#include "pcsysex.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace g1app
{
	using namespace pcsysex;

	namespace
	{
		constexpr uint64_t BootMs = 2500;		// the OS is up and has looked at its flash
		constexpr uint64_t QuietMs = 500;		// the editor's own exchanges come closer together
		constexpr uint64_t ReplyMs = 1500;
		constexpr uint64_t WriteMs = 300;		// the OS takes them without a word; then they are read back
		constexpr uint64_t FilterTailMs = 300;	// late replies to an abandoned request

		constexpr uint8_t CcIAm = 0x00, CcAck = 0x16, CcPatch = 0x17, CcSettings = 0x1f;
		constexpr uint8_t SectionType = 3;
		constexpr uint8_t ChannelMarker = 27;	// after each slot's channel, as the OS writes it
		// After the channels the OS adds what it calls by the PatchManager commands' numbers:
		// SlotsSelected 0:4 A:1 B:1 C:1 D:1, SlotActivated 0:6 slot:2, then each slot's voices
		// (5 and four bytes, which nothing here needs).
		constexpr uint8_t SlotsSelected = 0x07, SlotActivated = 0x09;
		constexpr uint8_t PatchManager = 0x41;

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
	}

	// ____________________________________________________________________________________________
	// The section: type 3, then clock source:1 vel min:7, LEDs:1 vel max:7, bpm:8, local:1
	// keyboard mode:1 pedal:1 global sync:5, master tune:8, PC receive:1 PC send:1 knob mode:1 0:5,
	// the name (up to 16 characters, ended by a zero when shorter), and per slot 0:3 channel:5 27:8;
	// then, from the OS, the slots on and the active one (SlotsSelected, SlotActivated).

	std::vector<uint8_t> SynthSettings::encode(const bool _withSlots) const
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
		if(_withSlots && hasSlots)
		{
			uint8_t mask = 0;
			for(size_t i = 0; i < 4; ++i)
				if(slotSelected[i])
					mask |= static_cast<uint8_t>(1 << (3 - i));
			s.insert(s.end(), {SlotsSelected, mask, SlotActivated, static_cast<uint8_t>(activeSlot & 3)});
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
		if(p + 4 <= _s.size() && _s[p] == SlotsSelected && _s[p + 2] == SlotActivated)
		{
			for(size_t i = 0; i < 4; ++i)
				r.slotSelected[i] = (_s[p + 1] >> (3 - i)) & 1;
			r.activeSlot = _s[p + 3] & 3;
			r.hasSlots = true;
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

	void SynthSettingsLink::write(const SynthSettings& _settings, const bool _withSlots)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_toWrite = _settings;
		m_writeSlots = _withSlots && _settings.hasSlots;
		m_writeWanted = true;
		m_pidFresh = false;		// read first: slot A may hold another patch since the last read
	}

	bool SynthSettingsLink::settings(SynthSettings& _out, uint64_t& _revision) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		_out = m_settings;
		_revision = m_revision;
		return m_known;
	}

	void SynthSettingsLink::reset()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_readWanted = true;
		m_writeWanted = false;
		m_known = false;
		m_pidFresh = false;
		m_state = State::Idle;
		m_greeted = false;
		m_pid = 0;
		m_deadline = m_filterUntil = m_lastActivity = m_lastRead = 0;
		m_packets.clear();
		m_rx.clear();
		m_editorRx.clear();
	}

	// ____________________________________________________________________________________________
	// Worker thread

	void SynthSettingsLink::finish(const uint64_t _nowMs)
	{
		m_state = State::Idle;
		m_filterUntil = _nowMs + FilterTailMs;
		m_packets.clear();
	}

	void SynthSettingsLink::editorSent(const std::vector<uint8_t>& _bytes, const uint64_t _nowMs)
	{
		m_editorRx.insert(m_editorRx.end(), _bytes.begin(), _bytes.end());
		forEachMessage(m_editorRx, [&](const std::vector<uint8_t>& _m)
		{
			// A greeting is not an exchange to keep out of (PresetsLink::editorSent).
			if(isClavia(_m) && ccOf(_m) == CcIAm)
				return;
			m_lastActivity = _nowMs;
			// The editor talks: the link gets out of the way and tries again once it is quiet.
			// The editor's own settings ($1F) are read back too, once it is quiet, if they are kept.
			if(m_state == State::Reading || (isClavia(_m) && ccOf(_m) == CcSettings && m_rereadMs > 0))
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_readWanted = true;
				if(m_state == State::Reading)
					finish(_nowMs);
			}
		});
	}

	void SynthSettingsLink::g1Sent(const std::vector<uint8_t>& _bytes, const uint64_t _nowMs, std::vector<uint8_t>& _toEditor)
	{
		m_rx.insert(m_rx.end(), _bytes.begin(), _bytes.end());
		forEachMessage(m_rx, [&](const std::vector<uint8_t>& _m)
		{
			if(!isClavia(_m))
			{
				_toEditor.insert(_toEditor.end(), _m.begin(), _m.end());
				return;
			}
			const bool hide = hides(ccOf(_m), _nowMs);	// before the reply moves the state on
			takeReply(_m, _nowMs);
			if(hide)
				traceMsg("<-", _nowMs, _m);
			else
				_toEditor.insert(_toEditor.end(), _m.begin(), _m.end());
		});
	}

	// The answers to the link's own requests, while one is open or shortly after: not the editor's.
	bool SynthSettingsLink::hides(const uint8_t _cc, const uint64_t _nowMs) const
	{
		const bool filtering = m_state != State::Idle || _nowMs < m_filterUntil;
		return (filtering && (isPacket(_cc) || _cc == CcAck)) || (_cc == CcIAm && m_state == State::Greeting);
	}

	void SynthSettingsLink::takeReply(const std::vector<uint8_t>& _m, const uint64_t _nowMs)
	{
		const auto cc = ccOf(_m);
		if(m_state == State::Greeting && cc == CcIAm && _m.size() > 4 && _m[4] == 0x01)
			return finish(_nowMs);		// m_readWanted is still set: tick() asks again
		if(m_state != State::Reading || !isPacket(cc) || _m.size() < 7)
			return;
		if(cc & 1)		// the first packet
			m_packets.clear();
		m_pid = static_cast<uint8_t>(_m[4] & 0x3f);
		m_packets.insert(m_packets.end(), _m.begin() + 5, _m.end() - 2);	// after cmd/pid, before checksum
		if(!(cc & 2))	// not the last one yet
			return;
		SynthSettings s;
		if(SynthSettings::decode(unpack7(m_packets), s))
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			// With a write waiting, this read was for the pid: what it says is about to be replaced,
			// and shown it would flash the old values on the page until the write is read back.
			if(!m_writeWanted)
			{
				m_settings = s;
				m_known = true;
				++m_revision;
			}
			m_pidFresh = true;
			finish(_nowMs);
		}
		m_packets.clear();
	}

	void SynthSettingsLink::tick(const uint64_t _nowMs, std::vector<uint8_t>& _toG1)
	{
		if(m_state == State::Idle)
		{
			if(const auto reread = m_rereadMs.load(); reread > 0 && m_lastRead > 0 && _nowMs >= m_lastRead + reread)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_readWanted = true;
			}
			if(_nowMs >= BootMs && _nowMs >= m_filterUntil && _nowMs >= m_lastActivity + QuietMs)
				startNext(_nowMs, _toG1);
			return;
		}
		if(_nowMs < m_deadline)
			return;
		const bool unanswered = m_state == State::Reading;
		finish(_nowMs);
		// No answer: the OS may not have met an editor yet. Greet it once, as one would.
		if(unanswered && !std::exchange(m_greeted, true))
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_readWanted = true;
			request({0xf0, 0x33, 0x00, 0x06, 0x00, 0x03, 0x03, 0xf7}, State::Greeting, ReplyMs, _nowMs, _toG1);	// IAm, as an editor
		}
	}

	// A write first, then a read: a write is read back to see what the OS made of it. A write
	// waits for a read that brings the pid it must carry (m_pidFresh).
	void SynthSettingsLink::startNext(const uint64_t _nowMs, std::vector<uint8_t>& _toG1)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto read = [&] { m_lastRead = _nowMs; request(frame(CcPatch, 0, {0x44, 0x02, 0x06, 0x08, 0x04}), State::Reading, ReplyMs, _nowMs, _toG1); };	// RequestSynthSettings, as NME
		if(m_writeWanted && !m_pidFresh)
			read();
		else if(m_writeWanted)
		{
			m_writeWanted = false;
			m_readWanted = true;
			auto payload = pack7(m_toWrite.encode(false));
			payload.insert(payload.begin(), m_pid);
			// The slots after the settings, as NME's slot buttons send them: the ones on, then the
			// active one, each on its own: sent right behind another message, the OS drops it.
			m_slotMessages.clear();
			if(m_writeSlots)
			{
				uint8_t mask = 0;
				for(size_t i = 0; i < 4; ++i)
					if(m_toWrite.slotSelected[i])
						mask |= static_cast<uint8_t>(1 << (3 - i));
				m_slotMessages.push_back(frame(CcPatch, 0, {PatchManager, SlotsSelected, mask}));
				m_slotMessages.push_back(frame(CcPatch, 0, {PatchManager, SlotActivated, static_cast<uint8_t>(m_toWrite.activeSlot & 3)}));
			}
			request(frame(0x1f, 0, payload), State::Writing, WriteMs, _nowMs, _toG1);
		}
		else if(!m_slotMessages.empty())
		{
			const auto msg = m_slotMessages.front();
			m_slotMessages.erase(m_slotMessages.begin());
			request(msg, State::Writing, WriteMs, _nowMs, _toG1);
		}
		else if(m_readWanted)
		{
			m_readWanted = false;
			read();
		}
	}

	// _msg to the G1, and the link waits in _state for up to _timeoutMs. What the G1 answers is
	// hidden from the editor until the transaction ends, and a little after (finish).
	void SynthSettingsLink::request(const std::vector<uint8_t>& _msg, const State _state, const uint64_t _timeoutMs, const uint64_t _nowMs, std::vector<uint8_t>& _toG1)
	{
		traceMsg("->", _nowMs, _msg);
		_toG1.insert(_toG1.end(), _msg.begin(), _msg.end());
		m_filterUntil = UINT64_MAX;
		m_state = _state;
		m_deadline = _nowMs + _timeoutMs;
	}
}
