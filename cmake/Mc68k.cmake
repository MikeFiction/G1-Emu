# 68331 fixes needed by the G1, applied only to a build copy of the file they touch. The
# Gearmulator clone remains the source and is never modified (see Dsp56300.cmake).
#
# Two GPT faults behind issue #22, both in the output compares the G1's OS times its internal
# master clock with (MIDIGlobal's clock, every synced sequencer):
# - The prescaler. The timer counts the system clock divided by what TMSK2's CPR bits select (/4
#   to /256, or PCLK), but Gearmulator's GPT always counts /4: TCNT is cycles >> 2 and a compare
#   matches after TOC << 2 cycles. The OS selects /32, so its ticks came 8 times too often. CPR 7
#   (PCLK, an external pin) is left at /4.
# - Matching by level. A compare matched whenever TCNT was past TOC with the flag down, so the
#   OS's handler, which clears OC2F before it moves TOC2 on, got a second interrupt at once: two
#   ticks back to back and then a gap of two periods, which a clock output merges into one pulse.
#   On the chip a compare matches once, when TCNT reaches TOC.
# Together the clock ran 4 times too fast; with both fixed it gives 24 pulses per beat at the
# synth's tempo, as the MIDI clock path always did.
get_target_property(g1_68k_source 68kEmu SOURCE_DIR)
set(g1_68k_overlay "${CMAKE_BINARY_DIR}/g1-68k")
file(READ "${g1_68k_source}/gpt.cpp" g1_gpt)

function(g1_gpt_replace before after)
	string(FIND "${g1_gpt}" "${before}" match)
	if(match EQUAL -1)
		message(FATAL_ERROR "Check the G1 fix for gpt.cpp: the external core has changed")
	endif()
	string(REPLACE "${before}" "${after}" g1_gpt "${g1_gpt}")
	set(g1_gpt "${g1_gpt}" PARENT_SCOPE)
endfunction()

g1_gpt_replace([=[		constexpr uint16_t g_tocCount = 2;]=]
	[=[		constexpr uint16_t g_tocCount = 2;

		// CPU cycles per TCNT tick, as a shift: TMSK2's CPR bits (the low byte of $YFF920) select
		// the system clock /4 (0) to /256 (6); 7 is PCLK, an external pin, taken as /4.
		uint32_t tcntShift(const uint16_t _tmsk)
		{
			const auto cpr = _tmsk & 7u;
			return cpr == 7 ? 2u : 2u + cpr;
		}]=])
g1_gpt_replace([=[		const auto tocTarget = static_cast<int32_t>(read16(tocAddr)) << 2;

		tocLoad += static_cast<int32_t>(_deltaCycles);

		if(tocLoad < tocTarget)
			return;]=]
	[=[		const auto tocTarget = static_cast<int32_t>(read16(tocAddr)) << tcntShift(PeripheralBase::read16(PeriphAddress::Tmsk1));

		const auto before = tocLoad;
		tocLoad += static_cast<int32_t>(_deltaCycles);

		// A compare matches when TCNT reaches TOC, once: not for as long as it is past it. The OS
		// clears OC2F before it moves TOC2 on, and in between a match by level fired again at once.
		if(before >= tocTarget || tocLoad < tocTarget)
			return;]=])
g1_gpt_replace([=[		const auto value = PeripheralBase::read16(tocAddr) << 2;

		auto& load = m_tocLoad[TocIndex];

		while(load > value)
			load -= 0x40000;

		while((value - load) >= 0x40000)
			load += 0x40000;]=]
	[=[		const auto shift = tcntShift(PeripheralBase::read16(PeriphAddress::Tmsk1));
		const auto value = PeripheralBase::read16(tocAddr) << shift;
		const int32_t wrap = 0x10000 << shift;

		auto& load = m_tocLoad[TocIndex];

		while(load > value)
			load -= wrap;

		while((value - load) >= wrap)
			load += wrap;]=])
g1_gpt_replace([=[		return (m_mc68k.getCycles() >> 2);]=]
	[=[		return m_mc68k.getCycles() >> tcntShift(const_cast<Gpt*>(this)->PeripheralBase::read16(PeriphAddress::Tmsk1));]=])

# The copy lives elsewhere: its own headers by their full path (an include directory would hide
# the system's <endian.h> from Musashi's C files behind mc68k's endian.h).
foreach(header gpt.h logging.h mc68k.h)
	g1_gpt_replace("#include \"${header}\"" "#include \"${g1_68k_source}/${header}\"")
endforeach()

file(WRITE "${g1_68k_overlay}/gpt.cpp.new" "${g1_gpt}")
configure_file("${g1_68k_overlay}/gpt.cpp.new" "${g1_68k_overlay}/gpt.cpp" COPYONLY)

get_target_property(g1_68k_sources 68kEmu SOURCES)
list(TRANSFORM g1_68k_sources REPLACE "^(.*/)?gpt\\.cpp$" "${g1_68k_overlay}/gpt.cpp")
set_property(TARGET 68kEmu PROPERTY SOURCES "${g1_68k_sources}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g1_68k_source}/gpt.cpp")
