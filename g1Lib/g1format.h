#pragma once

// A parameter's value as the editor shows it ("Sine", "1.25 kHz", "C#4") instead of the raw number
// the OS keeps. Ported from Animatek NME's ValueFormatters (source/format/ValueFormatters.cpp,
// itself a port of Nomad's nmformat.js), with the table of which parameter reads which way taken
// from NME's data/modules.xml (Nomad's module descriptions). Both GPL, as G1-Emu is.
//
// The text is the editor's, not the OS's own: it can differ in places from what the G1's display
// shows for the same value.

#include <cstdint>
#include <string>

namespace g1
{
	// _type is the module type (7 = OscA, as KnobInfo::type), _param its parameter's index. A
	// parameter with no particular reading is its number.
	std::string formatValue(uint8_t _type, uint8_t _param, int _value);
}
