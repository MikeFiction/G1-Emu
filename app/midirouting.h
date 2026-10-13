#pragma once

// What the hosts do to the MIDI on its way to the G1's MIDI IN, where the rack's OS alone would
// not do what the keyboard model does.
//
// Keyboard Mode set to Selected Slots (the synth settings), which on the keyboard model layers
// the selected slots on its own keys and on the rack only the editor's Keyboard Floater: a note
// on any selected slot's channel is played by every selected slot, whichever is active, each on
// its own channel, and what goes with the notes (sustain, pitch bend, the mod wheel, aftertouch,
// the channel mode messages) goes to them all too. Other controllers stay on their channel, as on
// the hardware (each slot receives on its own channel), unless MIDI CC Mode, the hosts' own
// setting, is Selected Slots: then every controller on a selected slot's channel goes to each
// selected slot, whatever Keyboard Mode is, and each patch answers its own assignments. Bank
// Select never: it goes with a Program Change, which stays on its channel.
//
// It runs on the thread that runs the G1 (EmuHost's, the plugin's Runner), which reads the
// active slot and the selected ones (their LEDs) straight from the G1.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace g1
{
	class Microcontroller;
}

namespace g1app
{
	class SynthSettingsLink;

	class MidiRouting
	{
	public:
		// What the synth settings say about where notes go: the slots' MIDI channels (0-15, 16 =
		// off) and Keyboard Mode. layout() has them as _settings last read them from the OS (its
		// defaults, channels 1 to 4 and Active Slot, until then or without it), and MIDI CC Mode, the
		// host's own setting (no OS has it).
		struct Layout
		{
			std::array<int, 4> channels{0, 1, 2, 3};
			bool selectedSlots = false;			// Keyboard Mode: Selected Slots
			bool selectedControllers = false;	// MIDI CC Mode: Selected Slots
		};
		static Layout layout(const SynthSettingsLink* _settings, bool _selectedControllers);

		// _in, on its way to the G1's MIDI IN, into _out, with Selected Slots played by each
		// selected slot.
		void apply(g1::Microcontroller& _mc, const Layout& _layout, const uint8_t* _in, size_t _size, std::vector<uint8_t>& _out);

	private:
		static constexpr int MaxCopies = 4;
		struct Target { uint8_t channel; int slot; };	// a copy of a message: its channel, the slot it plays (-1: none)
		struct Sent											// what a note played went to
		{
			bool played = false;	// a note on is held
			int count = 0;
			std::array<uint8_t, MaxCopies> channel{}, note{};
		};

		void message(g1::Microcontroller& _mc, const Layout& _layout, std::vector<uint8_t>& _out);
		int targets(g1::Microcontroller& _mc, const Layout& _layout, int _channel, bool _layered, std::array<Target, MaxCopies>& _out) const;

		uint8_t m_status = 0;		// running status: the last channel message's
		bool m_sysex = false;		// between $F0 and $F7, everything goes through
		uint8_t m_data[2] = {};
		int m_count = 0;
		std::array<std::array<Sent, 128>, 16> m_sent{};	// per channel and note played
	};
}
