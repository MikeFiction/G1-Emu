#pragma once

// The synth's banks, for the panel's Presets page: the names in each of the nine banks of 99, read
// from the OS the way an editor reads them, and a patch loaded from a bank into a slot.
//
// The list: PatchHandling ($17) with GetPatchList ($41 $14, bank and position). The OS answers
// with an ACK of type $13 or $15 carrying as many names as fit, from that position on, with codes
// for the positions it skips; the next request starts where that answer ended, until the bank is
// done. The load: LoadPatch ($41 $0A, slot, bank and position), which the OS answers with an ACK
// and then reports to the editor as a new patch in that slot, as when NME loads one.
//
// PresetsLink speaks to the OS through the PC Port like SynthSettingsLink: between the editor's
// messages, hiding the answers to its own requests from the editor. It runs no thread and owns no
// G1: the runner feeds it what the editor sent, what the G1 sent and the G1's time, and gives the
// G1 what the link wants to send.

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace g1app
{
	class PresetsLink
	{
	public:
		static constexpr int Banks = 9, Positions = 99;
		using BankNames = std::array<std::string, Positions>;	// empty: nothing stored there

		// The names in one answer to GetPatchList (the ACK's bytes after its type and pid, without
		// the checksum), starting at _bank/_position. _next is where the next request starts, or
		// -1 when the OS says the list is over.
		struct Entry { int bank; int position; std::string name; };
		static std::vector<Entry> decodeList(const std::vector<uint8_t>& _content, int _bank, int _position, int& _nextBank, int& _nextPosition);

		// Any thread. readBank() asks the OS for a bank's names (again, if it had them); load()
		// puts a stored patch into a slot (0-3).
		void readBank(int _bank);
		void load(int _slot, int _bank, int _position);
		// Uploads a patch into _slot as an editor does, each packet after the OS has acknowledged
		// the last one (_frames: PchUpload's), and then, when _bank >= 0, stores it at _bank and
		// _position. _abort ends a transfer cut short, so the OS does not stay waiting for the rest.
		// What came of it, and whether it is still going: uploadStatus().
		void upload(int _slot, std::vector<std::vector<uint8_t>> _frames, std::vector<uint8_t> _abort, int _bank, int _position);
		struct UploadStatus { bool busy = false, ok = false; std::string message; uint64_t serial = 0; };
		UploadStatus uploadStatus() const;

		// A bank's names as last read; false until it has been read once. The revision grows with
		// every change to any bank.
		bool bank(int _bank, BankNames& _out) const;
		bool reading(int _bank) const;
		uint64_t revision() const { return m_revision.load(); }
		// A new G1 (its time starts again at 0): whatever was in flight and what was read is
		// dropped. Only while no runner feeds the link.
		void reset();

		// Worker thread, with the engine lock held. _nowMs is the G1's own time (its CPU cycles).
		void editorSent(const std::vector<uint8_t>& _bytes, uint64_t _nowMs);
		void g1Sent(const std::vector<uint8_t>& _bytes, uint64_t _nowMs, std::vector<uint8_t>& _toEditor);
		void tick(uint64_t _nowMs, std::vector<uint8_t>& _toG1);

	private:
		enum class State { Idle, Listing, Loading, Uploading, Storing };

		void request(const std::vector<uint8_t>& _msg, State _state, uint64_t _nowMs, std::vector<uint8_t>& _toG1);
		void finish(uint64_t _nowMs);
		void takeReply(const std::vector<uint8_t>& _m, uint64_t _nowMs);
		bool hides(uint8_t _cc, uint64_t _nowMs) const;
		void uploadDone(bool _ok, const std::string& _message);

		mutable std::mutex m_mutex;		// what the other threads see and ask for
		std::array<BankNames, Banks> m_names{};
		std::array<bool, Banks> m_known{};
		std::array<bool, Banks> m_readWanted{};
		struct Load { int slot, bank, position; };
		std::vector<Load> m_loads;		// wanted, oldest first
		struct Upload { int slot = 0; std::vector<std::vector<uint8_t>> frames; std::vector<uint8_t> abort; int bank = -1, position = 0; };
		std::vector<Upload> m_uploads;	// wanted, oldest first
		UploadStatus m_uploadStatus;
		std::atomic<uint64_t> m_revision{0};
		std::atomic<int> m_readingBank{-1};

		std::atomic<State> m_state{State::Idle};
		BankNames m_partial{};			// the bank being read, filled as the answers come
		int m_bank = 0, m_position = 0;	// where the open request starts
		bool m_pendingNext = false;		// an answer came and the bank goes on: the next request, at the next tick
		Upload m_upload;				// the one going on
		size_t m_frame = 0;				// its next packet
		uint8_t m_uploadPid = 1;		// the id the OS gave the uploaded patch
		bool m_uploadNext = false;		// the last packet was acknowledged: the next one, or the store, at the next tick
		uint64_t m_deadline = 0;
		uint64_t m_filterUntil = 0;		// replies to the link are hidden from the editor until then
		uint64_t m_lastActivity = 0;	// the editor's last message
		std::vector<uint8_t> m_rx, m_editorRx;
		std::vector<uint8_t> m_toEditor;	// what the link has to tell the editor itself (a load's NewPatchInSlot)
	};
}
