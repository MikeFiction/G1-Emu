#include "g1cycleprofile.h"

#include "dsp56kEmu/disasm.h"
#include "dsp56kEmu/interrupts.h"
#include "dsp56kEmu/memory.h"
#include "dsp56kEmu/opcodecycles.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace g1
{
	CycleProfile::CycleProfile(dsp56k::DSP& _dsp)
		: m_dsp(_dsp), m_startCycles(_dsp.getCycles()), m_startInstructions(_dsp.getInstructionCounter())
	{
		const auto& c = m_dsp.getJit().getConfig();
		if(c.maxInstructionsPerBlock != 1 || c.linkJitBlocks)
			throw std::invalid_argument("cycle profiling requires G1_JITBLOCK=1 and unlinked blocks");
	}

	CycleProfile::Entry& CycleProfile::decode(const dsp56k::TWord _pc)
	{
		dsp56k::TWord op, ext;
		m_dsp.memory().getOpcode(_pc, op, ext);
		dsp56k::Instruction a, b;
		m_dsp.opcodes().getInstructionTypes(op, a, b);
		const auto len = dsp56k::Opcodes::getOpcodeLength(op, a, b);
		if(len == 1) ext = 0;
		const auto key = std::make_tuple(_pc, op, ext);
		auto it = m_entries.find(key);
		if(it != m_entries.end()) return it->second;
		dsp56k::Disassembler dis(m_dsp.opcodes());
		dsp56k::Disassembler::Line line;
		dis.disassemble(line, op, ext, m_dsp.regs().sr.var, m_dsp.regs().omr.var, _pc);
		const auto assembly = dsp56k::Disassembler::formatLine(line, false);
		auto mnemonic = line.opName;
		if(mnemonic.empty()) mnemonic = assembly.substr(0, assembly.find_first_of(" \t"));
		const auto cost = dsp56k::calcCycles(a, b, _pc, op, 0, 0);
		return m_entries.emplace(key, Entry{_pc, op, ext, len, a, b, cost, mnemonic, assembly}).first->second;
	}

	void CycleProfile::begin(const dsp56k::TWord _pc)
	{
		m_before = m_dsp.getCycles();
		m_first = &decode(_pc);
		const auto a = m_first->a;
		m_rep = a == dsp56k::Rep_ea || a == dsp56k::Rep_aa || a == dsp56k::Rep_xxx || a == dsp56k::Rep_S;
		// Snapshot opcodes BEFORE execution: a patch may rewrite P memory. REP bodies and
		// the second word of a fast vector are the only multi-op blocks in this mode.
		m_second = m_rep || (_pc < dsp56k::Vba_End && m_first->length == 1)
			? &decode(_pc + m_first->length) : nullptr;
	}

	void CycleProfile::beginVector(const dsp56k::TWord _pc)
	{
		if(m_first) ++m_errors;
		begin(_pc);
	}

	void CycleProfile::add(Entry& _entry, const uint64_t _count)
	{
		_entry.count += _count;
		_entry.cycles += _count * _entry.cost;
		m_instructions += _count;
		m_cycles += _count * _entry.cost;
	}

	void CycleProfile::finish()
	{
		if(!m_first) return;
		const auto elapsed = m_dsp.getCycles() - m_before;
		const auto accounted = m_cycles;
		add(*m_first, 1);
		if(m_rep && m_second && elapsed >= m_first->cost && m_second->cost)
		{
			// REP's body is counted once by block analysis and then N-1 more times in
			// the cycle counter. Derive its multiplicity from those emulated cycles,
			// not the core instruction counter (which currently counts N+2, not N+1).
			const auto repeats = (elapsed - m_first->cost) / m_second->cost;
			if(m_first->a == dsp56k::Rep_xxx)
			{
				const auto encoded = dsp56k::getFieldValue<dsp56k::Rep_xxx, dsp56k::Field_hhhh, dsp56k::Field_iiiiiiii>(m_first->op);
				if(repeats != (encoded ? encoded : 65536u)) ++m_errors;
			}
			add(*m_second, repeats);
		}
		else if(m_second && elapsed == m_first->cost + m_second->cost)
			add(*m_second, 1);
		if(m_cycles - accounted != elapsed) ++m_errors;
		m_first = m_second = nullptr;
	}

	void CycleProfile::exec()
	{
		m_dsp.execJitProfiled([this](const dsp56k::TWord pc)
		{
			finish(); // the interrupt vector, if one ran at this checkpoint
			begin(pc);
		}, [this] { finish(); });
	}

	bool CycleProfile::print(FILE* _out, const unsigned _dsp) const
	{
		using Count = std::pair<uint64_t, uint64_t>;
		std::map<std::string, Count> mnemonics;
		std::map<dsp56k::Instruction, Count> forms;
		std::vector<const Entry*> entries;
		for(const auto& [key, e] : m_entries)
			if(e.count) entries.push_back(&e);
		std::sort(entries.begin(), entries.end(), [](const Entry* a, const Entry* b)
		{
			return a->cycles != b->cycles ? a->cycles > b->cycles
				: std::tie(a->pc, a->op, a->extension) < std::tie(b->pc, b->op, b->extension);
		});
		const auto actual = m_dsp.getCycles() - m_startCycles;
		std::fprintf(_out, "CYCPROF dsp=%u cycles=%llu instructions=%llu core_instructions=%llu cycles_per_instruction=%.6f unaccounted_cycles=%lld errors=%llu\n",
			_dsp, static_cast<unsigned long long>(actual), static_cast<unsigned long long>(m_instructions),
			static_cast<unsigned long long>(m_dsp.getInstructionCounter() - m_startInstructions),
			m_instructions ? double(actual) / m_instructions : 0.0,
			static_cast<long long>(actual) - static_cast<long long>(m_cycles), static_cast<unsigned long long>(m_errors));
		for(const auto* e : entries)
		{
			std::fprintf(_out, "CYCPROF_PC dsp=%u pc=%06x op=%06x ext=%06x count=%llu cycles=%llu cpi=%u form_a=%u form_b=%u | %s\n",
				_dsp, e->pc, e->op, e->extension, static_cast<unsigned long long>(e->count),
				static_cast<unsigned long long>(e->cycles), e->cost, unsigned(e->a), unsigned(e->b), e->assembly.c_str());
			auto& m = mnemonics[e->mnemonic];
			m.first += e->count; m.second += e->cycles;
			for(const auto form : {e->a, e->b})
				if(form != dsp56k::Invalid)
				{
					auto& f = forms[form];
					f.first += e->count; f.second += e->cycles;
				}
		}
		std::vector<std::pair<std::string, Count>> totals(mnemonics.begin(), mnemonics.end());
		std::sort(totals.begin(), totals.end(), [](const auto& a, const auto& b)
		{ return a.second.second != b.second.second ? a.second.second > b.second.second : a.first < b.first; });
		for(const auto& [name, n] : totals)
			std::fprintf(_out, "CYCPROF_MNEMONIC dsp=%u mnemonic=%s count=%llu cycles=%llu\n", _dsp, name.c_str(),
				static_cast<unsigned long long>(n.first), static_cast<unsigned long long>(n.second));
		// Inclusive totals: both halves of a parallel instruction are listed; do NOT sum them.
		for(const auto& [form, n] : forms)
		{
			const auto& c = dsp56k::g_cycles[form];
			std::fprintf(_out, "CYCPROF_FORM dsp=%u form=%u count=%llu inclusive_cycles=%llu table=%u,%u,%u,%u | %s\n",
				_dsp, unsigned(form), static_cast<unsigned long long>(n.first), static_cast<unsigned long long>(n.second),
				c.cycles, c.pru, c.lab, c.lim, dsp56k::g_opcodes[form].m_assembly);
		}
		return m_errors == 0 && actual == m_cycles;
	}
}
