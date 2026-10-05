#pragma once

// The OS's synth settings (the panel's System menu, NME's Synth Settings dialog): the slots' MIDI
// channels, the clock, the velocity scale, master tune and the rest, read and written the way an
// editor does. A request (PatchHandling $44) brings them back as a patch packet carrying a
// section of type 3; the same section sent back (cc $1F) is what the OS takes and keeps in its
// flash.
//
// SynthSettingsLink speaks to the OS through the PC Port, between the editor's messages, and hides
// its own traffic from the editor, as the SlotKeeper does. It runs no thread and owns no G1: the
// runner feeds it what the editor sent, what the G1 sent and the G1's time, and gives the G1 what
// the link wants to send.

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace g1app
{
	struct SynthSettings
	{
		bool clockInternal = true;
		int velScaleMin = 0, velScaleMax = 127;
		bool ledsActive = true;
		int clockBpm = 120;
		bool localOn = true;
		int keyboardMode = 0;		// 0 active slot, 1 selected slots
		int pedalPolarity = 0;		// 0 normal, 1 inverted
		int globalSync = 0;			// in beats, 0-31
		int masterTune = 0;			// cents, -127 to 127
		bool programChangeReceive = true, programChangeSend = true;
		int knobMode = 0;			// 0 immediate, 1 hook
		std::string name;
		std::array<int, 4> midiChannel{0, 1, 2, 3};	// 0-15 per slot, 16 = off

		// The section as the OS sends and takes it (type 3 and its fields), 8-bit.
		std::vector<uint8_t> encode() const;
		static bool decode(const std::vector<uint8_t>& _section, SynthSettings& _out);
	};

	class SynthSettingsLink
	{
	public:
		// Any thread. read() asks the OS again; write() sends these and then reads them back.
		void read();
		void write(const SynthSettings& _settings);
		// What the OS said last; false until it has said anything. The revision grows with each.
		bool settings(SynthSettings& _out, uint64_t& _revision) const;
		// A new G1 (its time starts again at 0): whatever was in flight is dropped and it reads
		// them again. Only while no runner feeds the link.
		void reset();

		// Worker thread, with the engine lock held. _nowMs is the G1's own time (its CPU cycles).
		void editorSent(const std::vector<uint8_t>& _bytes, uint64_t _nowMs);
		void g1Sent(const std::vector<uint8_t>& _bytes, uint64_t _nowMs, std::vector<uint8_t>& _toEditor);
		void tick(uint64_t _nowMs, std::vector<uint8_t>& _toG1);

	private:
		enum class State { Idle, Greeting, Reading, Writing };

		void send(const std::vector<uint8_t>& _msg, uint64_t _nowMs, std::vector<uint8_t>& _toG1);
		void finish(uint64_t _nowMs);

		mutable std::mutex m_mutex;		// what the other threads see and ask for
		SynthSettings m_settings;
		uint64_t m_revision = 0;
		bool m_readWanted = true;		// once, as soon as the G1 is up
		bool m_writeWanted = false;
		SynthSettings m_toWrite;

		std::atomic<State> m_state{State::Idle};
		bool m_greeted = false;			// an unanswered request gets one IAm, as an editor's would
		uint8_t m_pid = 0;				// as the OS's last reply had it
		uint64_t m_deadline = 0;
		uint64_t m_filterUntil = 0;		// replies to the link are hidden from the editor until then
		uint64_t m_lastActivity = 0;	// the editor's last message
		std::vector<uint8_t> m_packets;	// a reply's packets so far (7-bit)
		std::vector<uint8_t> m_rx, m_editorRx;
	};
}
