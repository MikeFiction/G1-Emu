// g1formattest: a few parameters' values as g1::formatValue reads them, against what NME's
// editor shows for them (or the G1's display, where NME has no reading). No ROM needed. Exit
// code 1 on any failure.

#include "g1Lib/g1format.h"

#include <cstdio>
#include <string>

int main()
{
	struct Case { int type, param, value; const char* expected; };
	const Case cases[] = {
		{7, 4, 0, "Sine"}, {7, 4, 3, "Square"},		// OscA: waveform
		{7, 0, 60, "C4"}, {7, 0, 61, "C#4"},		// OscA: freq coarse
		{7, 1, 64, "0"}, {7, 1, 70, "6"},			// OscA: freq fine (value-64)
		{4, 1, 2, "CVA"},							// 2Output: destination
		{37, 0, 0, "1 ms"},							// LogicDelay: time
		{7, 9, 1, "On"},							// OscA: Mute
		{7, 5, 42, "42"},							// OscA: pitch mod 1, no reading of its own
		{20, 3, 1, "0.5"}, {20, 3, 125, "62.5"},	// ADSR: sustain, as the G1's display (value / 2)
		{20, 3, 126, "63"}, {20, 3, 127, "64"},		// ... and 127 is 64, not 63.5
		{23, 2, 64, "32"},							// Mod-Env: sustain, the same
		{97, 0, 36, "C2"},							// MasterOsc: pitch, as the G1's display (#38)
		{95, 0, 60, "C4"}, {96, 0, 61, "C#4"},		// PercOsc and FormantOsc: pitch, the same
		{107, 0, 36, "C2"},							// SpectralOsc: freq coarse, the same, not NME's Hz
		{200, 0, 5, "5"},							// no such module
	};
	int failed = 0;
	for(const auto& c : cases)
	{
		const auto got = g1::formatValue(static_cast<uint8_t>(c.type), static_cast<uint8_t>(c.param), c.value);
		const bool ok = got == c.expected;
		std::printf("%s type %d param %d value %d: \"%s\"%s\n", ok ? "ok  " : "FAIL", c.type, c.param, c.value, got.c_str(),
			ok ? "" : (std::string(", expected \"") + c.expected + "\"").c_str());
		failed += ok ? 0 : 1;
	}
	// The pitches whose display box switches to Hz, and that reading (NME's fmtOscHz).
	struct HzCase { int value; const char* expected; };
	for(const auto& c : {HzCase{69, "440 Hz"}, HzCase{36, "65.4 Hz"}, HzCase{0, "8.18 Hz"}, HzCase{127, "12.54 kHz"}})
	{
		const auto got = g1::formatHz(c.value);
		const bool ok = got == c.expected;
		std::printf("%s Hz value %d: \"%s\"%s\n", ok ? "ok  " : "FAIL", c.value, got.c_str(),
			ok ? "" : (std::string(", expected \"") + c.expected + "\"").c_str());
		failed += ok ? 0 : 1;
	}
	const bool hz = g1::hasHzReading(107, 0) && g1::hasHzReading(7, 0) && !g1::hasHzReading(107, 1) && !g1::hasHzReading(20, 0);
	std::printf("%s which pitches have a Hz reading\n", hz ? "ok  " : "FAIL");
	failed += hz ? 0 : 1;
	std::printf(failed ? "%d failed\n" : "all good\n", failed);
	return failed ? 1 : 0;
}
