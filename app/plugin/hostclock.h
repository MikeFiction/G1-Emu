#pragma once

// HostClock: the host's transport as MIDI clock for the G1's MIDI IN (issue #20).
//
// A VST3 host carries no MIDI clock to a plugin, so the plugin makes it from the transport: with
// the G1's MIDI clock set to external (the synth settings; a new flash comes up that way), this is
// what moves MIDIGlobal and everything clocked by it, at the host's tempo and on its beats.
//
// While the host plays, a $F8 on every 24th of a beat. When it starts: a Start ($FA) at the very
// beginning of the song, otherwise a Song Position Pointer to the sixteenth it starts in and a
// Continue ($FB). When it stops: a Stop ($FC). A jump while playing (a loop, a click on the ruler)
// just carries on with the ticks of the new place, so the tempo never stumbles.
//
// No allocation: it runs in the audio callback.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace g1app
{
	class HostClock
	{
	public:
		struct Event
		{
			uint32_t offset;	// frames into the block
			uint8_t size;
			uint8_t bytes[3];
		};
		static constexpr size_t MaxEvents = 256;	// 24 ticks a beat: a 16384-frame block at 44.1 kHz and 999 BPM is 148

		struct Block
		{
			std::array<Event, MaxEvents> events;
			size_t count = 0;
		};

		// One host block: whether the transport runs, its tempo and its position in quarter notes
		// at the block's first frame. _block gets this block's events, in order.
		void process(const bool _playing, const double _bpm, const double _ppq, const uint32_t _frames, const double _rate, Block& _block)
		{
			_block.count = 0;
			if(!_playing || !(_bpm > 0) || !(_rate > 0) || _frames == 0)
			{
				if(m_playing)
					add(_block, 0, {0xfc}, 1);
				m_playing = false;
				return;
			}

			const double ticksPerFrame = _bpm * 24.0 / 60.0 / _rate;
			const double first = _ppq * 24.0;			// in ticks
			const double end = first + ticksPerFrame * _frames;

			if(!m_playing)
			{
				if(_ppq < 1e-6)
					add(_block, 0, {0xfa}, 1);
				else
				{
					const auto sixteenth = static_cast<uint32_t>(std::floor(_ppq * 4.0 + 1e-6)) & 0x3fff;
					add(_block, 0, {0xf2, static_cast<uint8_t>(sixteenth & 0x7f), static_cast<uint8_t>(sixteenth >> 7)}, 3);
					add(_block, 0, {0xfb}, 1);
				}
				m_nextTick = static_cast<int64_t>(std::ceil(first - 1e-6));
				m_playing = true;
			}
			// A jump: anything but the position this block was expected at, give or take a tick.
			else if(std::abs(first - static_cast<double>(m_nextTick)) > 1.0)
				m_nextTick = static_cast<int64_t>(std::ceil(first - 1e-6));

			while(static_cast<double>(m_nextTick) < end - 1e-9)
			{
				const double at = (static_cast<double>(m_nextTick) - first) / ticksPerFrame;
				const auto offset = static_cast<uint32_t>(std::fmin(std::fmax(at, 0.0), static_cast<double>(_frames - 1)));
				if(!add(_block, offset, {0xf8}, 1))
					break;
				++m_nextTick;
			}
		}

		bool playing() const { return m_playing; }

	private:
		static bool add(Block& _block, const uint32_t _offset, const std::array<uint8_t, 3>& _bytes, const uint8_t _size)
		{
			if(_block.count == MaxEvents)
				return false;
			auto& e = _block.events[_block.count++];
			e.offset = _offset;
			e.size = _size;
			for(size_t i = 0; i < 3; ++i)
				e.bytes[i] = _bytes[i];
			return true;
		}

		bool m_playing = false;
		int64_t m_nextTick = 0;
	};
}
