// g1synthsettingstest: the SynthSettingsLink against an emulated G1, with no editor.
//
//   1. The link reads the factory settings once the G1 is up (greeting it, as nothing else has).
//   2. It writes other MIDI channels and globals, and reading them back gives what was written.
//   3. A section survives encode() and decode() as it was.
//
// Exit code 77 (skipped) without a ROM, 1 on any failure.

#include "engine.h"
#include "hostconfig.h"
#include "romfinder.h"
#include "synthsettings.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
	constexpr uint64_t g_ms = g1::g_ucClock / 1000;

	std::string describe(const g1app::SynthSettings& _s)
	{
		char buf[256];
		std::snprintf(buf, sizeof(buf), "\"%s\" channels %d %d %d %d, clock %s %d bpm, sync %d, tune %d, vel %d-%d, local %d, LEDs %d, kb %d, pedal %d, PC %d/%d, knob %d",
			_s.name.c_str(), _s.midiChannel[0] + 1, _s.midiChannel[1] + 1, _s.midiChannel[2] + 1, _s.midiChannel[3] + 1,
			_s.clockInternal ? "internal" : "external", _s.clockBpm, _s.globalSync, _s.masterTune, _s.velScaleMin, _s.velScaleMax,
			_s.localOn, _s.ledsActive, _s.keyboardMode, _s.pedalPolarity, _s.programChangeReceive, _s.programChangeSend, _s.knobMode);
		return buf;
	}

	bool same(const g1app::SynthSettings& _a, const g1app::SynthSettings& _b)
	{
		return _a.clockInternal == _b.clockInternal && _a.velScaleMin == _b.velScaleMin && _a.velScaleMax == _b.velScaleMax
			&& _a.ledsActive == _b.ledsActive && _a.clockBpm == _b.clockBpm && _a.localOn == _b.localOn
			&& _a.keyboardMode == _b.keyboardMode && _a.pedalPolarity == _b.pedalPolarity && _a.globalSync == _b.globalSync
			&& _a.masterTune == _b.masterTune && _a.programChangeReceive == _b.programChangeReceive
			&& _a.programChangeSend == _b.programChangeSend && _a.knobMode == _b.knobMode && _a.name == _b.name
			&& _a.midiChannel == _b.midiChannel;
	}
}

int main()
{
	g1app::HostOptions options;
	options.load(g1app::defaultSettingsPath());
	if(const char* v = std::getenv("G1_ROM"))
		options.rom = v;
	const auto search = g1app::findRom({}, options.rom);
	std::vector<uint8_t> rom;
	if(!search.found() || !g1app::inspectRom(search.path, rom).ok())
	{
		std::printf("skipped: no ROM\n");
		return 77;
	}

	int failures = 0;
	auto check = [&](const bool _ok, const std::string& _what)
	{
		std::printf("%s %s\n", _ok ? "ok  " : "FAIL", _what.c_str());
		if(!_ok)
			++failures;
	};

	std::string osNote;
	g1app::Engine engine(rom, options.loadOs(osNote));
	auto& mc = engine.mc();
	g1app::SynthSettingsLink link;
	std::vector<uint8_t> fromG1, toEditor, toG1;
	auto run = [&](const uint64_t _ms)
	{
		for(uint64_t i = 0; i < _ms; ++i)
		{
			const auto end = mc.ucCycles() + g_ms;
			while(mc.ucCycles() < end)
				mc.exec();
			const auto now = mc.ucCycles() / g_ms;
			fromG1.clear();
			mc.getPcPort().takeTx(fromG1);
			toEditor.clear();
			link.g1Sent(fromG1, now, toEditor);
			toG1.clear();
			link.tick(now, toG1);
			if(!toG1.empty())
				mc.getPcPort().receive(toG1);
		}
	};
	auto waitFor = [&](const uint64_t _revision, const uint64_t _maxMs, g1app::SynthSettings& _s)
	{
		uint64_t rev = 0;
		for(uint64_t t = 0; t < _maxMs; t += 50)
		{
			run(50);
			if(link.settings(_s, rev) && rev > _revision)
				return rev;
		}
		return uint64_t(0);
	};

	// 1. The factory settings
	g1app::SynthSettings factory;
	const auto rev1 = waitFor(0, 8000, factory);
	check(rev1 > 0, "read the settings: " + describe(factory));

	// 2. Other channels and globals, and back
	auto changed = factory;
	changed.midiChannel = {4, 9, 15, 2};
	changed.clockBpm = 97;
	changed.globalSync = 8;
	changed.masterTune = -12;
	changed.knobMode = 1;
	changed.velScaleMax = 100;
	link.write(changed);
	g1app::SynthSettings back;
	const auto rev2 = waitFor(rev1, 5000, back);
	check(rev2 > rev1, "read back after writing: " + describe(back));
	check(same(back, changed), "the OS keeps what was written");

	// 3. encode/decode
	g1app::SynthSettings round;
	check(g1app::SynthSettings::decode(changed.encode(), round) && same(round, changed), "encode/decode");

	std::printf(failures ? "%d failure(s)\n" : "all ok\n", failures);
	return failures ? 1 : 0;
}
