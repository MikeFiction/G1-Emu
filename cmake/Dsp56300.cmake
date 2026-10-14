# Core fixes needed by the G1, applied only to the build copy. The upstream clone
# (G1_DSP56300_DIR) remains the source and is never modified.
get_target_property(g1_dsp_source dsp56kEmu SOURCE_DIR)
set(g1_dsp_overlay "${CMAKE_BINARY_DIR}/g1-dsp/dsp56kEmu")
set(g1_dsp_prepare "${CMAKE_BINARY_DIR}/g1-dsp/prepare")
file(GLOB g1_dsp_files CONFIGURE_DEPENDS
	"${g1_dsp_source}/*.cpp" "${g1_dsp_source}/*.h" "${g1_dsp_source}/*.inl")
foreach(source IN LISTS g1_dsp_files)
	get_filename_component(name "${source}" NAME)
	configure_file("${source}" "${g1_dsp_prepare}/${name}" COPYONLY)
endforeach()

function(g1_dsp_replace name before after)
	set(path "${g1_dsp_prepare}/${name}")
	file(READ "${path}" contents)
	string(FIND "${contents}" "${before}" match)
	if(match EQUAL -1)
		message(FATAL_ERROR "Check the G1 fix for ${name}: the external core has changed")
	endif()
	string(REPLACE "${before}" "${after}" contents "${contents}")
	file(WRITE "${path}" "${contents}")
endfunction()

# The G1 runs one DO iteration per block. That made the mask of the iteration test zero, and
# AArch64 cannot encode an immediate of zero for TST: asmjit refused the instruction, the rest of
# the block was never emitted and the DSP ran into it, which is the illegal instruction on Apple
# Silicon. The test is always true with that mask, so the jump is unconditional.
g1_dsp_replace(jitblock.cpp
	"\t\t\tif(m_config.maxDoIterations)\n\t\t\t{\n\t\t\t\tassert(asmjit::Support::isPowerOf2(m_config.maxDoIterations));\n\t\t\t\tm_asm.test_(_regLC, asmjit::Imm(m_config.maxDoIterations-1));\n\t\t\t\tm_asm.jz(skip);\n\t\t\t}"
	"\t\t\tif(m_config.maxDoIterations > 1)\n\t\t\t{\n\t\t\t\tassert(asmjit::Support::isPowerOf2(m_config.maxDoIterations));\n\t\t\t\tm_asm.test_(_regLC, asmjit::Imm(m_config.maxDoIterations-1));\n\t\t\t\tm_asm.jz(skip);\n\t\t\t}\n\t\t\telse if(m_config.maxDoIterations)\n\t\t\t{\n\t\t\t\tm_asm.jmp(skip);\n\t\t\t}")

# A REP and the instruction it repeats are analysed as one unit now, but the REP still skipped the
# per-block instruction limit, a leftover of when its instruction came as the next one: the
# instruction after the pair joined the block too. With one instruction per block (G1_JITBLOCK=1,
# which cycle profiling needs) that block held three, and the profile charged the third to the REP.
g1_dsp_replace(jitblock.cpp
	"if(!isRep && !isFastInterrupt && _config.maxInstructionsPerBlock > 0 && numInstructions >= _config.maxInstructionsPerBlock)"
	"if(!isFastInterrupt && _config.maxInstructionsPerBlock > 0 && numInstructions >= _config.maxInstructionsPerBlock)")

# The ESSI clock converts its delay in cycles into instructions by the measured ratio, rounded to
# serve early, never late. It rounded the cycles per instruction up to a whole number: the G1's DSPs
# run ~2.1 cycles per instruction, taken as 3, so each wait ended at two thirds of the way and the
# clock was asked again, three or four times per tick of 48 cycles, a third of the emulator's time.
# The reciprocal is kept in fixed point anyway: computed from the counts themselves, rounded down,
# it is just as early and within one instruction of the tick.
g1_dsp_replace(esaiclock.cpp
	[=[			const auto cyclesPerInstruction = std::max(static_cast<uint64_t>(1), (deltaCycles + deltaInstructions - 1) / deltaInstructions);
			m_instructionsPerCycle = static_cast<uint32_t>((1u << g_ratioShift) / cyclesPerInstruction);]=]
	[=[			m_instructionsPerCycle = static_cast<uint32_t>(std::min<uint64_t>(1u << g_ratioShift, (deltaInstructions << g_ratioShift) / std::max<uint64_t>(deltaCycles, 1)));]=])

# A move to or from X:aa / Y:aa (short absolute address) takes one cycle. Upstream charges one for
# the read and two for the write, from a garbled row of the manual's Table A-1; the table here makes
# both one (the read is one either way, so nothing counts short). The G1's sample and control code
# is full of them (its variables live at X:$0-$3F): with two, a patch the OS had filled to its
# budget went past 864 cycles per sample and lost sample clocks, ~6,000 in 3 s on DSP 0 with
# BellsMorph.pch and noise 40 dB louder above 4 kHz. The OS was written for the real chip.
g1_dsp_replace(opcodecycles.h
	"OpcodeCycles{Movex_aa,			2, 0, 0, 0},"
	"OpcodeCycles{Movex_aa,			1, 0, 0, 0},")
g1_dsp_replace(opcodecycles.h
	"OpcodeCycles{Movey_aa,			2, 0, 0, 0},"
	"OpcodeCycles{Movey_aa,			1, 0, 0, 0},")

# When the slot being served is due, by the clock's own count: the DSP serves its peripherals when
# its instruction count says so, a few dozen cycles off either way. G1-Emu hands each link word
# the block that is being received at that moment, and near a block's edge the DSP's cycle count
# gave the next or the previous one: a sample out of place now and then, noise as loud as the
# signal's highs in patches that keep the DSPs busy (BellsMorph, WavetableSynth).
g1_dsp_replace(esaiclock.h
	[=[		void setTickCallback(TickCallback _callback) { m_tickCallback = std::move(_callback); }]=]
	[=[		void setTickCallback(TickCallback _callback) { m_tickCallback = std::move(_callback); }

		// G1-Emu: while a slot is served, when it was due (in the clock source's units)
		uint64_t getSlotClock() const { return m_lastClock; }]=])

# The clock serves one slot per call, so that the DSP can take each slot's request before the
# next. It is called between JIT blocks, and once a patch fills the G1's DSPs their blocks last
# longer than a slot (96 cycles) on average: the clock fell behind for good, ~28 % slower than the
# DSP with BellsMorph, and the links carried words from the wrong sample. Up to 8 slots per call
# now, while it is behind; the G1 moves its link words by DMA, which takes them as they come.
g1_dsp_replace(esaiclock.h
	[=[		uint32_t exec() noexcept;]=]
	[=[		uint32_t exec() noexcept;
		uint32_t execOne() noexcept;	// G1-Emu: one slot, see exec()]=])
g1_dsp_replace(esaiclock.cpp
	[=[	uint32_t EsxiClock::exec() noexcept
	{]=]
	[=[	uint32_t EsxiClock::exec() noexcept
	{
		uint32_t delay = 0;
		for(uint32_t i = 0; i < 8 && !delay; ++i)
			delay = execOne();
		return delay;
	}

	uint32_t EsxiClock::execOne() noexcept
	{]=])

# DMA per request in block mode without clearing DE (DTM=100). The G1's DSP 0 brings in the
# left audio input this way: DMA3 moves ESSI1 RX to X:$6C5 (a 1-word block) and its interrupt
# (vector $1E) copies ESSI0 RX to X:$6C4. The core takes block mode by request only when it
# clears DE. One request moves the whole block; with DE set, it stays armed.
g1_dsp_replace(dma.cpp
	"|| tm == TransferMode::BlockTriggerRequestClearDE;"
	"|| tm == TransferMode::BlockTriggerRequestClearDE || tm == TransferMode::BlockTriggerRequest;")
g1_dsp_replace(dma.cpp
	"		if(getTransferMode() == TransferMode::BlockTriggerRequestClearDE)\n		{\n			while(!execTransfer())"
	"		if(getTransferMode() == TransferMode::BlockTriggerRequestClearDE || getTransferMode() == TransferMode::BlockTriggerRequest)\n		{\n			while(!execTransfer())")

# External interrupts (the HI08's host commands) reach the pending queue only in execPeripherals,
# which runs only when no internal interrupt can be serviced. A DSP whose sample routine fills the
# block finds the next IRQD pending at every RTI and services it straight away, so a host command
# waited for ever and G1-Emu, after 200,000 cycles, dropped it: the 68k then polled for an answer
# that never came (#4, WavetableSynth.pch). They are now also collected when the next interrupt is
# picked, and queue behind what is already pending, as they would be arbitrated on the chip.
g1_dsp_replace(dsp.cpp
	[=[	void DSP::execInterrupts()
	{]=]
	[=[	void DSP::execInterrupts()
	{
		m_execPeripheralsFunc(this);	// G1-Emu: peripherals keep their time while interrupts keep coming
		processExternalInterrupts();	// G1-Emu: host commands must not wait for an idle DSP
]=])

# A DMA channel's interrupt is a status bit on the chip: a second end of transfer while the first
# is still waiting is the same request. The core queues every one, in order, and serviced none of
# them while a long interrupt (the sample routine) ran. DSP 0 ends a transfer of its audio input
# (DMA3, vector $1E) twice per sample, so with a patch the OS had filled to its budget the queue
# reached six of them, the next IRQD waited behind them, and the sample clock after it found it
# still pending and was lost: ~60 a second with BellsMorph.pch, a crackle. Its handler copies a
# register, so servicing it once loses nothing. A DMA interrupt is queued only if it is not
# pending already (the core merges them only while they are masked).
g1_dsp_replace(dma.cpp
	[=[		if(bitvalue(m_dcr, Die))
			m_peripherals.getDSP().injectInterrupt(Vba_DMAchannel0 + (m_index<<1));]=]
	[=[		if(bitvalue(m_dcr, Die) && !m_peripherals.getDSP().hasPendingInterrupt(Vba_DMAchannel0 + (m_index<<1)))
			m_peripherals.getDSP().injectInterrupt(Vba_DMAchannel0 + (m_index<<1));]=])

# Who is told when an interrupt is serviced: G1-Emu knows from it that the sample clock (IRQD) was
# taken, counts vectors for its diagnostics and attributes cycles to them when profiling.
g1_dsp_replace(dsp.h
	[=[		bool			injectInterruptImmediate		(uint32_t _interruptVectorAddress);]=]
	[=[		bool			injectInterruptImmediate		(uint32_t _interruptVectorAddress);
		void			setInterruptServicedCallback	(std::function<void(TWord)> _cb) { m_interruptServicedCallback = std::move(_cb); }
		std::function<void(TWord)>		m_interruptServicedCallback;]=])
g1_dsp_replace(dsp.cpp
	[=[	void DSP::execInterrupt(const TWord vba)
	{
		pcCurrentInstruction = vba;]=]
	[=[	void DSP::execInterrupt(const TWord vba)
	{
		if(m_interruptServicedCallback)
			m_interruptServicedCallback(vba);	// G1-Emu
		pcCurrentInstruction = vba;]=])

# Cycle profiling dispatches separately, after the peripheral/interrupt checkpoint. The
# ordinary exec path has no profiling hook or test. Interrupt vectors are observed through
# the serviced callback above, so their cycles are not charged to the interrupted PC.
g1_dsp_replace(dsp.h
	[=[		ASMJIT_FORCE_INLINE void execInterpreter() noexcept]=]
	[=[		template<typename Before, typename After>
		void execJitProfiled(Before&& _before, After&& _after)
		{
			m_interruptFunc(this);
			const auto pc = getPC().toWord();
			_before(pc);
			m_jit.getTrampoline().execOne(&reg, pc, m_jitEntries[pc]);
			_after();
		}

		ASMJIT_FORCE_INLINE void execInterpreter() noexcept]=])

# Issue #17: a block the JIT cannot generate (asmjit out of memory, or code generation failing).
# create() left the "create" stub in place and executed it, which called create() again with no
# end: a stack overflow, and in a plugin the whole host down. Now create() returns, the JIT keeps
# a sticky failure (and why) for the G1's Dsp to see, and a child that could not be made is not
# linked. G1_JIT_FAIL_AT=n fails the n-th block emitted (counted over all DSPs), and
# G1_JIT_FAIL_FROM=n every block from the n-th on, to test that path.
g1_dsp_replace(jit.h
	[=[#include <memory>]=]
	[=[#include <memory>
#include <string>]=])
g1_dsp_replace(jit.h
	[=[		void checkPMemWrite() noexcept;]=]
	[=[		void checkPMemWrite() noexcept;
	public:
		void setFailure(const char* _reason) { if(m_failure.empty()) m_failure = _reason; }
		bool hasFailed() const { return !m_failure.empty(); }
		const std::string& failReason() const { return m_failure; }
		void clearFailure() { m_failure.clear(); }
	private:
		std::string m_failure;]=])
g1_dsp_replace(jitblockchain.cpp
	[=[#include "jitblockemitter.h"]=]
	[=[#include "jitblockemitter.h"

#include <atomic>
#include <cstdlib>]=])
g1_dsp_replace(jitblockchain.cpp
	[=[		emit(_pc);

		if(m_generatingBlocks.empty()]=]
	[=[		if(!emit(_pc))
			return;	// the stub left in place would only call create() again

		if(m_generatingBlocks.empty()]=])
g1_dsp_replace(jitblockchain.cpp
	[=[		auto* emitter = m_jit.acquireEmitter(_pc);

//		m_logger->addFlags]=]
	[=[		{
			static const long failAt = std::getenv("G1_JIT_FAIL_AT") ? std::atol(std::getenv("G1_JIT_FAIL_AT")) : 0;
			static const long failFrom = std::getenv("G1_JIT_FAIL_FROM") ? std::atol(std::getenv("G1_JIT_FAIL_FROM")) : 0;
			static std::atomic<long> emitted{0};
			if(failAt > 0 || failFrom > 0)
			{
				const auto n = ++emitted;
				if(n == failAt || (failFrom > 0 && n >= failFrom))
				{
					m_jit.setFailure("simulated (G1_JIT_FAIL_AT / G1_JIT_FAIL_FROM)");
					return nullptr;
				}
			}
		}
		auto* emitter = m_jit.acquireEmitter(_pc);

//		m_logger->addFlags]=])
g1_dsp_replace(jitblockchain.cpp
	[=[			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			m_jit.releaseBlockRuntimeData(b);]=]
	[=[			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			m_jit.setFailure("code generation failed");
			m_jit.releaseBlockRuntimeData(b);]=])
g1_dsp_replace(jitblockchain.cpp
	[=[			LOG("JIT failed: " << err << " - " << errString << "PC " << HEX(_pc));
			m_jit.releaseEmitter(emitter);]=]
	[=[			LOG("JIT failed: " << err << " - " << errString << "PC " << HEX(_pc));
			m_jit.setFailure(errString);
			m_jit.releaseEmitter(emitter);]=])
g1_dsp_replace(jitblockchain.cpp
	[=[		create(_pc, false);

		if (!canBeDefaultExecuted(_pc))
			return nullptr;]=]
	[=[		create(_pc, false);

		if (!m_jitCache[_pc].block || !canBeDefaultExecuted(_pc))
			return nullptr;	// not generated (#17): the parent calls the stub instead of linking]=])

foreach(source IN LISTS g1_dsp_files)
	get_filename_component(name "${source}" NAME)
	configure_file("${g1_dsp_prepare}/${name}" "${g1_dsp_overlay}/${name}" COPYONLY)
endforeach()

get_target_property(g1_dsp_sources dsp56kEmu SOURCES)
set(g1_dsp_build_sources)
foreach(source IN LISTS g1_dsp_sources)
	get_filename_component(name "${source}" NAME)
	if(EXISTS "${g1_dsp_overlay}/${name}")
		list(APPEND g1_dsp_build_sources "${g1_dsp_overlay}/${name}")
	else()
		list(APPEND g1_dsp_build_sources "${source}")
	endif()
endforeach()
set_property(TARGET dsp56kEmu PROPERTY SOURCES "${g1_dsp_build_sources}")
target_include_directories(dsp56kEmu BEFORE PUBLIC "${CMAKE_BINARY_DIR}/g1-dsp")
target_include_directories(dsp56kEmu PRIVATE "${g1_dsp_source}")
