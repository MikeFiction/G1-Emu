// g1hostgesturestest: knobs turned on the panel, as the host is told of them (issue #42), with no
// DAW: one begin per turn, its values, one end once it has stopped, whatever the timer's pace.

#include "hostgestures.h"

#include <cstdio>
#include <string>

namespace
{
	int g_failed = 0;
	void expect(const bool _ok, const char* _what)
	{
		std::printf("%s: %s\n", _ok ? "ok" : "FAILED", _what);
		if(!_ok)
			g_failed = 1;
	}

	using G = g1plugin::HostGestures<19>;
	struct Count { int begins = 0, values = 0, ends = 0; };
	Count count(const std::vector<G::Event>& _e, const size_t _index)
	{
		Count c;
		for(const auto& e : _e)
			if(e.index == _index)
				(e.kind == G::Kind::Begin ? c.begins : e.kind == G::Kind::Value ? c.values : c.ends)++;
		return c;
	}
}

int main()
{
	// A turn: knob 3 moves at every tick of a 33 ms timer for a second, then rests.
	{
		G g;
		std::vector<G::Event> ev;
		uint32_t t = 1000;
		for(int i = 0; i < 30; ++i, t += 33)
		{
			g.changed(3, static_cast<float>(i) / 30.0f, t, ev);
			g.idle(t, ev);
		}
		expect(count(ev, 3).begins == 1 && count(ev, 3).values == 30 && count(ev, 3).ends == 0, "one begin and every value while it turns, no end");
		expect(!ev.empty() && ev.front().kind == G::Kind::Begin, "the begin comes before its first value");
		for(int i = 0; i < 12; ++i, t += 33)
			g.idle(t, ev);
		expect(count(ev, 3).ends == 1 && !g.open(3), "one end once it has rested for 300 ms");
	}

	// Two turns with a pause between: two gestures. Two knobs at once: one each.
	{
		G g;
		std::vector<G::Event> ev;
		g.changed(0, 0.1f, 0, ev);
		g.changed(7, 0.5f, 0, ev);
		g.changed(0, 0.2f, 100, ev);
		g.idle(500, ev);
		g.changed(0, 0.3f, 900, ev);
		g.idle(1300, ev);
		expect(count(ev, 0).begins == 2 && count(ev, 0).ends == 2, "two turns of knob 1, two gestures");
		expect(count(ev, 7).begins == 1 && count(ev, 7).ends == 1, "knob 8 turned with it, its own gesture");
	}

	// The editor closes with a knob still turning: its gesture ends.
	{
		G g;
		std::vector<G::Event> ev;
		g.changed(18, 0.7f, 0, ev);
		g.closeAll(ev);
		expect(count(ev, 18).begins == 1 && count(ev, 18).ends == 1 && !g.open(18), "closeAll ends the open turn");
	}

	std::printf("%s\n", g_failed ? "FAILED" : "passed");
	return g_failed;
}
