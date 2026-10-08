// g1presetstest: the PresetsLink, the synth's banks for the Presets page.
//
//   1. decodeList on a hand-made answer: names, an empty position skipped, the end of the list.
//   2. Against an emulated G1, with a flash that has patches (G1_FLASH, or the standalone's
//      flash.bin): bank 1 is read, and loading its first patch into slot A puts that patch's name
//      on the G1's display, as a load from the panel would.
//
// Exit code 77 (skipped) without a ROM or a flash with patches for part 2, 1 on any failure.

#include "engine.h"
#include "hostconfig.h"
#include "presetslink.h"
#include "romfinder.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
	constexpr uint64_t g_ms = g1::g_ucClock / 1000;

	std::string trim(std::string _s)
	{
		while(!_s.empty() && _s.back() == ' ')
			_s.pop_back();
		return _s;
	}
}

int main()
{
	int failures = 0;
	auto check = [&](const bool _ok, const std::string& _what)
	{
		std::printf("%s %s\n", _ok ? "ok  " : "FAIL", _what.c_str());
		if(!_ok)
			++failures;
	};

	// 1. A hand-made answer: three unknown bytes, "Abc" at 5, an empty position, "D" at 7, the end.
	{
		const std::vector<uint8_t> content = {6, 22, 1, 'A', 'b', 'c', 0, 2, 'D', 0, 4};
		int nb = 0, np = 0;
		const auto e = g1app::PresetsLink::decodeList(content, 0, 5, nb, np);
		check(e.size() == 2 && e[0].position == 5 && e[0].name == "Abc" && e[1].position == 7 && e[1].name == "D" && nb == -1,
			"decodeList: names, an empty position, the end");
		// Sixteen characters have no zero after them.
		const std::vector<uint8_t> full = {6, 22, 1, 'S','i','x','t','e','e','n',' ','c','h','a','r','s','!','!','!', 'X', 0, 0};
		const auto f = g1app::PresetsLink::decodeList(full, 2, 0, nb, np);
		check(f.size() == 2 && f[0].name == "Sixteen chars!!!" && f[1].name == "X" && f[1].position == 1 && nb == 2 && np == 2,
			"decodeList: a 16-character name, then the next, and where to go on");
	}

	g1app::HostOptions options;
	options.load(g1app::defaultSettingsPath());
	if(const char* v = std::getenv("G1_ROM"))
		options.rom = v;
	const auto search = g1app::findRom({}, options.rom);
	std::vector<uint8_t> rom;
	const char* flashPath = std::getenv("G1_FLASH");
	const std::string flashFile = flashPath ? flashPath : g1app::defaultFlashPath();
	std::ifstream ff(flashFile, std::ios::binary);
	const std::vector<uint8_t> flash((std::istreambuf_iterator<char>(ff)), std::istreambuf_iterator<char>());
	if(!search.found() || !g1app::inspectRom(search.path, rom).ok() || flash.empty())
	{
		std::printf("%s; part 2 skipped: no ROM or no flash with patches\n", failures ? "FAILED" : "part 1 ok");
		return failures ? 1 : 77;
	}

	std::string osNote;
	g1app::Engine engine(rom, options.loadOs(osNote));
	check(engine.loadFlash(flash), "flash loaded from " + flashFile);
	auto& mc = engine.mc();
	g1app::PresetsLink link;
	std::vector<uint8_t> fromG1, toEditor, toG1, seen;	// seen: all that reached the editor
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
			seen.insert(seen.end(), toEditor.begin(), toEditor.end());
			toG1.clear();
			link.tick(now, toG1);
			if(!toG1.empty())
				mc.getPcPort().receive(toG1);
		}
	};

	// The OS answers an editor that has greeted it, as NME does first.
	run(2000);
	mc.getPcPort().receive({0xf0, 0x33, 0x00, 0x06, 0x00, 0x03, 0x03, 0xf7});
	run(500);

	link.readBank(0);
	g1app::PresetsLink::BankNames names;
	bool known = false;
	for(int t = 0; t < 200 && !known; ++t)
	{
		run(50);
		known = link.bank(0, names);
	}
	int used = 0, first = -1;
	for(int p = 0; p < g1app::PresetsLink::Positions; ++p)
		if(!names[static_cast<size_t>(p)].empty())
		{
			++used;
			if(first < 0)
				first = p;
		}
	check(known, "bank 1 read: " + std::to_string(used) + " of 99 positions used");
	if(first < 0)
	{
		std::printf("bank 1 is empty: the load is not checked\n");
		return failures ? 1 : 77;
	}
	for(int p = 0, shown = 0; p < g1app::PresetsLink::Positions && shown < 6; ++p)
		if(!names[static_cast<size_t>(p)].empty())
		{
			std::printf("     %02d %s\n", p + 1, names[static_cast<size_t>(p)].c_str());
			++shown;
		}

	seen.clear();
	link.load(0, 0, first);
	std::string shown;
	for(int t = 0; t < 60; ++t)
	{
		run(50);
		shown = trim(mc.getLcd().line(0));
		if(shown.find(trim(names[static_cast<size_t>(first)])) != std::string::npos)
			break;
	}
	check(shown.find(trim(names[static_cast<size_t>(first)])) != std::string::npos,
		"loaded position " + std::to_string(first + 1) + " into slot A: the display says \"" + shown + "\"");

	// The editor is told, as after a load from the panel: NewPatchInSlot (cc $14, sc $38) for slot A.
	bool told = false;
	for(size_t i = 0; i + 7 < seen.size(); ++i)
		told = told || (seen[i] == 0xf0 && seen[i + 1] == 0x33 && (seen[i + 2] >> 2) == 0x14 && seen[i + 5] == 0x38 && seen[i + 6] == 0);
	check(told, "the editor is told of the new patch in slot A (NewPatchInSlot)");

	std::printf(failures ? "%d failure(s)\n" : "all ok\n", failures);
	return failures ? 1 : 0;
}
