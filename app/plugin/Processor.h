#pragma once

// The G1 as a VST3 instrument: notes from the track, four outputs (1/2 and 3/4, like the back
// panel), two inputs, and the G1's user state (patches and synth settings, never the OS) inside
// the DAW project. The emulator runs on the Runner's thread, paced by the host's blocks.
//
// Creating an instance creates no audio client and no file: everything goes through the host. The
// one thing it creates is a virtual MIDI port, "G1-Emu PC Port" ("G1-Emu 2 PC Port" for the second
// instance, and so on), so an editor (Animatek NME) can reach this G1 as it reaches the hardware's
// PC Port. JUCE can make one on Linux and macOS; on Windows it cannot, and the instance says so.
// It lives as long as the instance, across engine swaps, so the editor stays connected. The ROM is found like the standalone finds it (romfinder.h); the only file this ever
// writes is the settings file, and only when the user picks a ROM by hand.
//
// A new instance, with nothing in the project yet, starts from a copy of the standalone's flash
// when there is one, so the banks and the patches in the slots are the ones already there. It is
// a copy: the standalone's file is never written from here.

#include "runner.h"
#include "jucemidi.h"
#include "g1Lib/g1knobs.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace g1plugin
{
	// One of the 18 panel knobs as a host parameter. Its name says what the knob moves in the
	// patch now ("Knob 3: OscA Freq coars"), and its text is the value the OS gives that parameter,
	// both read from the OS's tables (g1knobs.h) and changed when another patch comes.
	class KnobParameter final : public juce::AudioParameterFloat
	{
	public:
		explicit KnobParameter(int _index);
		juce::String getName(int _maxLength) const override;
		bool setInfo(const g1::KnobInfo& _info);		// true if the name changed

		// The parameter's 0..1 against the knob's position (ADC). Positions 0 and 255 change
		// nothing in the OS, so the parameter's range is 1..254: its ends are the parameter's ends.
		static float toParam(uint8_t _adc) { return juce::jlimit(0.0f, 1.0f, (static_cast<float>(_adc) - 1.0f) / 253.0f); }
		static uint8_t toAdc(float _v) { return static_cast<uint8_t>(1 + juce::roundToInt(juce::jlimit(0.0f, 1.0f, _v) * 253.0f)); }

	private:
		juce::String getText(float _v, int _maxLength) const override;
		float getValueForText(const juce::String& _text) const override;
		const int m_index;
		mutable std::mutex m_mutex;
		juce::String m_name;
		g1::KnobInfo m_info;
	};

	class Processor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer
	{
	public:
		Processor();
		~Processor() override;

		void prepareToPlay(double _rate, int _maxBlock) override;
		void releaseResources() override;
		bool isBusesLayoutSupported(const BusesLayout& _layouts) const override;
		void processBlock(juce::AudioBuffer<float>& _buffer, juce::MidiBuffer& _midi) override;
		using juce::AudioProcessor::processBlock;

		juce::AudioProcessorEditor* createEditor() override;
		bool hasEditor() const override { return true; }

		const juce::String getName() const override { return "G1-Emu"; }
		bool acceptsMidi() const override { return true; }
		bool producesMidi() const override { return false; }
		bool isMidiEffect() const override { return false; }
		double getTailLengthSeconds() const override { return 0.0; }

		// VST3 carries no Program Change as MIDI: a host that has one for the plugin sets its
		// program parameter instead. These 128 programs are Program Changes on channel 1 (slot A's
		// channel by default), so a Program Change from the track reaches the G1 either way.
		int getNumPrograms() override { return 128; }
		int getCurrentProgram() override { return m_programs[0].program.load() < 0 ? 0 : m_programs[0].program.load(); }
		void setCurrentProgram(int _index) override;
		const juce::String getProgramName(int _index) override { return "Program " + juce::String(_index + 1); }
		void changeProgramName(int, const juce::String&) override {}

		void getStateInformation(juce::MemoryBlock& _dest) override;
		void setStateInformation(const void* _data, int _size) override;

		// For the editor, on the message thread. The engine can be replaced (a project's state
		// arriving reboots the G1 with its flash): generation() changes when it is, and the
		// editor drops its panel before the old one goes (Editor::engineGoing).
		g1app::Engine* engine();
		int generation() const { return m_generation.load(); }
		g1app::HostStats stats();
		const std::string& romProblem() const { return m_romProblem; }
		std::string describe();		// for the Settings box: ROM, latency, where the banks came from

		// The ROM picked by hand: remembered in the settings file, like the standalone does.
		void useRom(const juce::File& _file);

		bool extrasOpen() const { return m_extrasOpen; }
		void setExtrasOpen(bool _open) { m_extrasOpen = _open; }
		bool knobDisplays() const { return m_knobDisplays; }
		void setKnobDisplays(bool _on) { m_knobDisplays = _on; }

	private:
		void handleAsyncUpdate() override;

		// The 18 knobs both ways: the host's automation turns them (processBlock), and what turns
		// them otherwise (the panel, Random, a project's state) is passed to the host (the timer).
		// m_knobMutex keeps the two apart; processBlock only tries it, and if the timer has it,
		// leaves the knobs for the next block.
		void timerCallback() override;
		void knobsFromHost();
		void knobsFromEngine();
		std::array<KnobParameter*, 18> m_knobParams{};
		std::array<int, 18> m_lastAdc{};				// the position each knob was last seen at
		std::array<float, 18> m_lastParam{};			// the value each parameter was last seen at
		std::mutex m_knobMutex;
		int m_knobGeneration = -1;
		void findRom();
		// Makes the engine from the state (or, with none, from the standalone's flash) and starts
		// it if the host is playing. Message thread, or before anything runs.
		void createEngine(const juce::MemoryBlock* _state);
		void destroyEngine();
		void startRunner();
		bool applyState(g1app::Engine& _engine, const juce::MemoryBlock& _state);
		juce::MemoryBlock snapshotState();
		juce::MemoryBlock settingsOnlyState();
		void startFromStandalone(g1app::Engine& _engine);

		// The PC Port's virtual MIDI port. Declared before the engine and the runner so it goes after them.
		std::unique_ptr<juce::InterProcessLock> m_instanceLock;	// holds m_instance's number for every process
		int m_instance = 0;						// 1 for the first live instance, 2 for the next...
		std::unique_ptr<g1app::JuceMidi> m_pcPort;
		int m_pcIndex = -1;
		std::string m_pcProblem;				// why there is no PC Port, when there is none
		void openPcPort();

		std::vector<uint8_t> m_rom;
		std::string m_romPath, m_romProblem;

		std::mutex m_lifecycle;					// m_engine and m_runner are swapped under it
		std::unique_ptr<g1app::Engine> m_engine;
		std::unique_ptr<g1app::Runner> m_runner;
		std::unique_ptr<g1app::SlotKeeper> m_keeper;	// what each slot holds; lives as long as m_engine
		std::atomic<int> m_generation{0};
		std::string m_origin;					// where this instance's flash came from

		// A state the host gave before there was an engine (or with no ROM at all): it is what
		// gets used, and what gets handed back, so a project opened without the ROM loses nothing.
		juce::MemoryBlock m_state;
		bool m_haveState = false;
		bool m_keepProjectState = false;		// the project's G1 state could not be used: hand it back as it came
		std::mutex m_pendingMutex;
		juce::MemoryBlock m_pending;			// set off the message thread, applied on it

		double m_rate = 0;
		int m_maxBlock = 0;
		bool m_prepared = false;
		float m_gainDb = 36.0f;					// undoes the -36 dB cap the OS puts on the master volume
		juce::AudioBuffer<float> m_inputs;		// the inputs, copied before the outputs overwrite them

		bool m_extrasOpen = false, m_knobDisplays = false;

		// The last Bank Select (CC 0 and 32) and Program Change the track sent on each channel.
		// The OS does not keep which patch each slot had, so a G1 booting from a saved project
		// would come up with empty slots: these are sent again as soon as it starts. -1: none.
		struct Program { std::atomic<int> bankMsb{-1}, bankLsb{-1}, program{-1}; };
		std::array<Program, 16> m_programs;
		bool m_unstarted = false;				// a new instance's G1 that has not run yet (settingsOnlyState)
		bool m_engineFresh = false;				// created and not started yet: replay the programs
		std::atomic<int> m_hostProgram{-1};		// set by the host's program parameter, sent by processBlock
		void replayPrograms();
		std::string programsToString() const;
		void programsFromString(const juce::String& _text);
	};
}
