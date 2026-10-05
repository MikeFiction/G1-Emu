#pragma once

// DirectLink: the PC Port's bytes over a local TCP socket, for Animatek NME (issue #8).
//
// Beside the MIDI PC Port, never instead of it: the original Clavia editor and every other tool
// still reach the G1 over MIDI. NME, which is ours, can also find a running emulator here and talk
// to it with no MIDI port, no driver and no loopMIDI, which is what makes the editor usable with
// the plugin and on Windows.
//
// The wire is the PC Port as it is: raw MIDI bytes both ways, no framing of our own. Each instance
// listens on 127.0.0.1, on the first free port from kBasePort up, so instance n is found at
// kBasePort + n. On accepting an editor it sends one line first, "G1-Emu <version> <name>\n", so the
// editor knows it reached an emulator and which one before any SysEx.
//
// One editor at a time: a second connection is accepted and closed at once. No threads: poll() and
// send() are called from the loop that already moves the PC Port's bytes, and never block.

#include <cstdint>
#include <string>
#include <vector>

namespace g1app
{
	class DirectLink
	{
	public:
		static constexpr int kBasePort = 47310;
		static constexpr int kMaxInstances = 8;
		static constexpr int kProtocolVersion = 1;

		DirectLink() = default;
		~DirectLink();
		DirectLink(const DirectLink&) = delete;
		DirectLink& operator=(const DirectLink&) = delete;

		// Listens on the first free port from kBasePort, announcing itself as _name. False, with
		// the reason in _error, when no port is free or sockets do not work here.
		bool start(const std::string& _name, std::string& _error);
		void stop();

		bool listening() const { return m_listen >= 0; }
		bool editorConnected() const { return m_client >= 0; }
		int port() const { return m_port; }   // 0 while not listening
		const std::string& name() const { return m_name; }
		// The name sent to the next editor that connects (an instance learns its number from its port).
		void setName(const std::string& _name) { m_name = _name; }

		// Accepts a waiting editor and appends whatever it sent to _in.
		void poll(std::vector<uint8_t>& _in);

		// Sends to the connected editor, if there is one.
		void send(const std::vector<uint8_t>& _bytes);

		// One line for the status bar.
		std::string describe() const;

	private:
		void closeClient();

		long long m_listen = -1;   // socket handles: int on POSIX, SOCKET on Windows
		long long m_client = -1;
		int m_port = 0;
		std::string m_name;
	};
}
