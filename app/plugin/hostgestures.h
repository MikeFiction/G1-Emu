#pragma once

// HostGestures: knobs turned on the panel, told to the host as a person turning them would be
// (issue #42). A host learns, records and undoes by gestures: one begin when a knob starts to move,
// its values while it moves, one end once it has stopped. The plugin used to send a whole
// begin-value-end at every timer tick of a turn, thirty a second: to a host learning a parameter
// (Cubase's and Nuendo's modulators, "Acquisition") that is thirty touches, and Nuendo crashed.
//
// Plain bookkeeping, no JUCE and no threads: the message thread feeds it the changes it sees and
// the time, and it says what to send.

#include <array>
#include <cstdint>
#include <vector>

namespace g1plugin
{
	template<size_t N> class HostGestures
	{
	public:
		static constexpr uint32_t HoldMs = 300;		// still for this long: the turn is over

		enum class Kind { Begin, Value, End };
		struct Event { size_t index; Kind kind; float value; };

		// _index moved to _value: a begin first if it was still.
		void changed(const size_t _index, const float _value, const uint32_t _nowMs, std::vector<Event>& _out)
		{
			if(_index >= N)
				return;
			if(!m_open[_index])
			{
				_out.push_back({_index, Kind::Begin, _value});
				m_open[_index] = true;
			}
			_out.push_back({_index, Kind::Value, _value});
			m_last[_index] = _nowMs;
		}

		// The turns that have been still for HoldMs end.
		void idle(const uint32_t _nowMs, std::vector<Event>& _out)
		{
			for(size_t i = 0; i < N; ++i)
				if(m_open[i] && _nowMs - m_last[i] >= HoldMs)
				{
					_out.push_back({i, Kind::End, 0.0f});
					m_open[i] = false;
				}
		}

		// Every open turn ends now (the engine goes, the editor closes).
		void closeAll(std::vector<Event>& _out)
		{
			for(size_t i = 0; i < N; ++i)
				if(m_open[i])
				{
					_out.push_back({i, Kind::End, 0.0f});
					m_open[i] = false;
				}
		}

		bool open(const size_t _index) const { return _index < N && m_open[_index]; }

	private:
		std::array<bool, N> m_open{};
		std::array<uint32_t, N> m_last{};
	};
}
