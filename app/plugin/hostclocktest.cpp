// g1hostclocktest: the host's transport as MIDI clock (issue #20), with no ROM and no DAW.
//
// A transport played through blocks of random sizes must give one $F8 every 24th of a beat, on
// the frame where it falls, with nothing doubled or lost at the blocks' edges; a Start at the
// song's beginning, a Song Position Pointer and a Continue elsewhere, a Stop when it stops, and
// after a loop's jump the ticks of the new place, again with nothing doubled.

#include "hostclock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace
{
	constexpr double g_rate = 48000.0;

	struct Seen
	{
		uint64_t frame;
		uint8_t size;
		uint8_t b0, b1, b2;
	};

	int g_failed = 0;

	void expect(const bool _ok, const char* _what)
	{
		std::printf("%s: %s\n", _ok ? "ok" : "FAILED", _what);
		if(!_ok)
			g_failed = 1;
	}

	// Plays from _ppq for _frames at _bpm in random blocks (or _fixed ones), appending what comes out.
	double play(g1app::HostClock& _clock, std::vector<Seen>& _seen, uint64_t& _frame, double _ppq, const uint32_t _frames,
		const double _bpm, std::mt19937& _rng, const uint32_t _fixed = 0)
	{
		g1app::HostClock::Block block;
		uint32_t done = 0;
		while(done < _frames)
		{
			const uint32_t n = std::min(_frames - done, _fixed ? _fixed : std::uniform_int_distribution<uint32_t>(1, 2048)(_rng));
			_clock.process(true, _bpm, _ppq, n, g_rate, block);
			for(size_t i = 0; i < block.count; ++i)
			{
				const auto& e = block.events[i];
				_seen.push_back({_frame + e.offset, e.size, e.bytes[0], e.bytes[1], e.bytes[2]});
			}
			_ppq += _bpm / 60.0 / g_rate * n;
			_frame += n;
			done += n;
		}
		return _ppq;
	}

	size_t count(const std::vector<Seen>& _seen, const uint8_t _status)
	{
		size_t n = 0;
		for(const auto& s : _seen)
			n += s.b0 == _status;
		return n;
	}

	// Every $F8 where tick k of a transport started at frame _start and tick _firstTick falls.
	bool ticksInPlace(const std::vector<Seen>& _seen, const uint64_t _start, const int64_t _firstTick, const double _startPpq, const double _bpm)
	{
		const double framesPerTick = g_rate * 60.0 / _bpm / 24.0;
		int64_t k = _firstTick;
		for(const auto& s : _seen)
		{
			if(s.b0 != 0xf8)
				continue;
			const double want = static_cast<double>(_start) + (static_cast<double>(k) - _startPpq * 24.0) * framesPerTick;
			if(std::abs(static_cast<double>(s.frame) - want) > 0.5 + 1e-6)	// the nearest frame
			{
				std::printf("    tick %lld at frame %llu, expected %.1f\n", static_cast<long long>(k),
					static_cast<unsigned long long>(s.frame), want);
				return false;
			}
			++k;
		}
		return true;
	}
}

int main()
{
	std::mt19937 rng(std::getenv("G1_SEED") ? static_cast<unsigned>(std::atoi(std::getenv("G1_SEED"))) : 20261007u);	// G1_SEED: other blocks

	// From the beginning, at 120 BPM: a Start, then 48 ticks a second.
	{
		g1app::HostClock clock;
		std::vector<Seen> seen;
		uint64_t frame = 0;
		play(clock, seen, frame, 0.0, static_cast<uint32_t>(g_rate * 2), 120.0, rng);
		expect(!seen.empty() && seen.front().b0 == 0xfa && seen.front().frame == 0, "a Start first, at the first frame");
		expect(count(seen, 0xf8) == 96, "96 ticks in 2 s at 120 BPM, in random blocks");
		expect(ticksInPlace(seen, 0, 0, 0.0, 120.0), "each tick on its frame");
		g1app::HostClock::Block block;
		clock.process(false, 120.0, 4.0, 512, g_rate, block);
		expect(block.count == 1 && block.events[0].bytes[0] == 0xfc, "a Stop when the transport stops");
		clock.process(false, 120.0, 4.0, 512, g_rate, block);
		expect(block.count == 0, "nothing while stopped");
	}

	// The same in blocks of one frame, and of exactly one tick: nothing doubled or lost at the edges.
	for(const uint32_t fixed : {1u, 1000u, 999u})
	{
		g1app::HostClock clock;
		std::vector<Seen> seen;
		uint64_t frame = 0;
		play(clock, seen, frame, 0.0, static_cast<uint32_t>(g_rate), 120.0, rng, fixed);
		char what[96];
		std::snprintf(what, sizeof(what), "48 ticks in 1 s in blocks of %u frames, each on its frame", fixed);
		expect(count(seen, 0xf8) == 48 && ticksInPlace(seen, 0, 0, 0.0, 120.0), what);
	}

	// From the middle of bar 2 (4.5 quarter notes, the 18th sixteenth), at 93 BPM.
	{
		g1app::HostClock clock;
		std::vector<Seen> seen;
		uint64_t frame = 0;
		play(clock, seen, frame, 4.5, static_cast<uint32_t>(g_rate * 3), 93.0, rng);
		expect(seen.size() >= 2 && seen[0].b0 == 0xf2 && seen[0].b1 == 18 && seen[0].b2 == 0 && seen[1].b0 == 0xfb,
			"a Song Position Pointer (sixteenth 18) and a Continue");
		expect(count(seen, 0xfa) == 0, "no Start there");
		expect(ticksInPlace(seen, 0, 108, 4.5, 93.0), "the ticks of the place it started, on their frames");
	}

	// A loop: 4 bars from bar 2, played twice. After the jump the ticks start again from the
	// loop's beginning, with no Start, no Stop and nothing doubled.
	{
		g1app::HostClock clock;
		std::vector<Seen> seen;
		uint64_t frame = 0;
		const auto loopFrames = static_cast<uint32_t>(16 * 60.0 / 120.0 * g_rate);	// 16 beats
		play(clock, seen, frame, 4.0, loopFrames, 120.0, rng, 480);
		const size_t firstPass = count(seen, 0xf8);
		play(clock, seen, frame, 4.0, loopFrames, 120.0, rng, 480);
		expect(firstPass == 16 * 24 && count(seen, 0xf8) == 2 * 16 * 24, "16 beats of ticks on each pass of the loop");
		expect(count(seen, 0xfa) == 0 && count(seen, 0xfc) == 0 && count(seen, 0xfb) == 1, "one Continue, no Start or Stop at the jump");
	}

	std::printf("%s\n", g_failed ? "FAILED" : "passed");
	return g_failed;
}
