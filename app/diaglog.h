#pragma once

// What the log needs to say where a "no sound" comes from (issue #65): the machine, and, a
// moment after MIDI arrives, how far the sound gets through the four DSPs. Shared by the
// standalone (EmuHost) and the plugin (Runner), which both write through hostlog.h.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace g1
{
	class Microcontroller;
}

namespace g1app
{
	// One line: the CPU, its cores (performance and efficiency ones on a hybrid CPU), the
	// instruction sets the DSP JIT picks code by, and whether the DSPs run on threads.
	std::string systemSummary(bool _dspThreads);

	// Logs a "health" line some 1.5 s after MIDI comes in: the output's peak and each link's
	// between DSPs over that time, the links' misses (a block the next DSP wanted and did not
	// have yet) and each DSP's state. At most one every 20 s, so a long session stays readable.
	class HealthProbe
	{
	public:
		// DSP 3's thread, every output sample (DC already removed): outputs 1 and 2.
		void output(int32_t _o1, int32_t _o2)
		{
			const int32_t a = std::max(_o1 < 0 ? -_o1 : _o1, _o2 < 0 ? -_o2 : _o2);
			if(a > m_peak.load(std::memory_order_relaxed))
				m_peak.store(a, std::memory_order_relaxed);
		}
		// The emulator's thread, with the engine to itself: MIDI has just gone into the G1.
		void midiIn(g1::Microcontroller& _mc, uint64_t _nowMs);
		// The emulator's thread, with the engine to itself, often.
		void tick(g1::Microcontroller& _mc, uint64_t _nowMs);

	private:
		std::atomic<int32_t> m_peak{0};
		bool m_pending = false, m_any = false;
		uint64_t m_dueMs = 0, m_lastMs = 0;
		std::array<uint64_t, 4> m_misses{};
	};
}
