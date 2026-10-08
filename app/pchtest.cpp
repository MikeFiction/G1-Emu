// g1pchtest: the Presets page's Load .pch against an emulated G1, with no editor.
//
// tools/patches/ClockTest.pch (or G1_PCH) is read and made into packets with Animatek NME's code (PchUpload),
// uploaded into slot A by the PresetsLink as an editor would, and stored in bank 9 at 99; then the
// bank is read again and must have its name there, and the G1's display shows it in slot A.
// With G1_FLASH (or the standalone's flash.bin) the banks start as that flash has them.
//
// Exit code 77 (skipped) without a ROM, 1 on any failure.

#include "engine.h"
#include "hostconfig.h"
#include "presetslink.h"
#include "romfinder.h"
#include "gui/PchUpload.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

int main()
{
	int failures = 0;
	auto check = [&](const bool _ok, const std::string& _what)
	{
		std::printf("%s %s\n", _ok ? "ok  " : "FAIL", _what.c_str());
		if(!_ok)
			++failures;
	};

	const char* pch = std::getenv("G1_PCH");	// another patch, a bigger one (several packets)
	const juce::File file(pch ? juce::String(pch) : juce::String(G1_SOURCE_DIR "/tools/patches/ClockTest.pch"));
	const auto up = g1gui::preparePch(file, 0);
	check(up.ok(), file.getFileName().toStdString() + " made into " + std::to_string(up.frames.size()) + " packets, named \"" + up.name.toStdString() + "\"" + (up.error.isEmpty() ? "" : ": " + up.error.toStdString()));

	g1app::HostOptions options;
	options.load(g1app::defaultSettingsPath());
	if(const char* v = std::getenv("G1_ROM"))
		options.rom = v;
	const auto search = g1app::findRom({}, options.rom);
	std::vector<uint8_t> rom;
	if(!search.found() || !g1app::inspectRom(search.path, rom).ok())
	{
		std::printf("no ROM: the upload is not tried\n");
		return failures ? 1 : 77;
	}
	std::string osNote;
	g1app::Engine engine(rom, options.loadOs(osNote));
	const char* flashPath = std::getenv("G1_FLASH");
	std::ifstream ff(flashPath ? flashPath : g1app::defaultFlashPath(), std::ios::binary);
	const std::vector<uint8_t> flash((std::istreambuf_iterator<char>(ff)), std::istreambuf_iterator<char>());
	if(!flash.empty())
		engine.loadFlash(flash);

	auto& mc = engine.mc();
	constexpr uint64_t ms = g1::g_ucClock / 1000;
	g1app::PresetsLink link;
	std::vector<uint8_t> fromG1, toEditor, toG1;
	auto run = [&](const uint64_t _ms)
	{
		for(uint64_t i = 0; i < _ms; ++i)
		{
			const auto end = mc.ucCycles() + ms;
			while(mc.ucCycles() < end)
				mc.exec();
			const auto now = mc.ucCycles() / ms;
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
	run(2000);
	mc.getPcPort().receive({0xf0, 0x33, 0x00, 0x06, 0x00, 0x03, 0x03, 0xf7});	// IAm, as an editor greets it
	run(500);

	link.upload(0, up.frames, up.abort, 8, 98);
	g1app::PresetsLink::UploadStatus status;
	for(int t = 0; t < 400; ++t)
	{
		run(50);
		status = link.uploadStatus();
		if(!status.busy && status.serial > 1)
			break;
	}
	check(status.ok && !status.busy, "uploaded and stored: " + status.message);

	g1app::PresetsLink::BankNames names;
	bool known = false;
	for(int t = 0; t < 200 && !known; ++t)
	{
		run(50);
		known = link.bank(8, names);
	}
	check(known && names[98] == up.name.toStdString(), "bank 9 read again: at 99 \"" + names[98] + "\"");
	std::string shown = mc.getLcd().line(0);
	while(!shown.empty() && shown.back() == ' ')
		shown.pop_back();
	check(shown.find(up.name.toStdString()) != std::string::npos, "the display shows \"" + shown + "\"");

	std::printf(failures ? "%d failure(s)\n" : "all ok\n", failures);
	return failures ? 1 : 0;
}
