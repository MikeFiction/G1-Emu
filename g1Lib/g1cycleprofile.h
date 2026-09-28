#pragma once

#include "dsp56kEmu/dsp.h"

#include <cstdio>
#include <map>
#include <string>
#include <tuple>

namespace g1
{
	// Diagnostic only. Construct with the DSP stopped, maxInstructionsPerBlock=1 and
	// block linking disabled; call beginVector from its interrupt-serviced callback.
	// Never used by the regular dispatcher, and never reads the host clock.
	class CycleProfile
	{
	public:
		explicit CycleProfile(dsp56k::DSP& _dsp);
		void exec();
		void beginVector(dsp56k::TWord _pc);
		bool print(FILE* _out, unsigned _dsp) const;
		uint64_t instructions() const { return m_instructions; }
		uint64_t cycles() const { return m_cycles; }
		uint64_t errors() const { return m_errors; }

	private:
		struct Entry
		{
			dsp56k::TWord pc, op, extension, length;
			dsp56k::Instruction a, b;
			uint32_t cost;
			std::string mnemonic, assembly;
			uint64_t count = 0, cycles = 0;
		};
		Entry& decode(dsp56k::TWord _pc);
		void begin(dsp56k::TWord _pc);
		void finish();
		void add(Entry& _entry, uint64_t _count);

		dsp56k::DSP& m_dsp;
		std::map<std::tuple<uint32_t, uint32_t, uint32_t>, Entry> m_entries;
		Entry* m_first = nullptr;
		Entry* m_second = nullptr;
		bool m_rep = false;
		uint64_t m_before = 0, m_startCycles, m_startInstructions;
		uint64_t m_cycles = 0, m_instructions = 0, m_errors = 0;
	};
}
