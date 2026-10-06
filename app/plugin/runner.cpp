#include "runner.h"

#include <algorithm>
#include <string>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace g1app
{
	namespace
	{
		// How far past its target the worker can end up: the audio arrives a DSP sync at a time
		// (some 5 samples at 96 kHz), and the loop only looks between syncs. 0.5 ms is plenty.
		size_t guardFor(const double _rate)
		{
			return std::max<size_t>(32, static_cast<size_t>(std::lround(_rate * 0.0005)));
		}

		// How far ahead the worker may run: a whole block, plus a margin for its own unevenness.
		size_t aheadFor(const double _rate, const size_t _maxBlock)
		{
			double marginMs = 5.0;
			if(const char* v = std::getenv("G1_PLUGIN_MARGIN_MS"))
				marginMs = std::max(0.0, std::atof(v));
			return std::max<size_t>(_maxBlock, 32) + static_cast<size_t>(std::lround(_rate * marginMs / 1000.0));
		}
	}

	Runner::Runner(Engine& _engine, const double _rate, const size_t _maxBlock, const float _gainDb,
		MidiTransport* _pcPort, const int _pcIndex, SlotKeeper* _keeper, DirectLink* _link)
		: m_engine(_engine), m_pcPort(_pcPort), m_link(_link), m_pcIndex(_pcIndex), m_keeper(_keeper), m_rate(_rate > 0 ? _rate : 48000.0), m_ahead(aheadFor(m_rate, _maxBlock)),
		  m_latency(m_ahead + guardFor(m_rate)),
		  m_bridge(std::pow(10.0f, _gainDb / 20.0f))
	{
		m_bridge.setRate(static_cast<uint32_t>(std::lround(m_rate)));

		auto& mc = m_engine.mc();
		mc.getDsp(3).setBlockCallback([this](const int32_t _o1, const int32_t _o2, const int32_t _o3, const int32_t _o4)
		{
			constexpr auto dc = Engine::OutputDc;
			m_bridge.push(_o1 - dc, _o2 - dc, _o3 - dc, _o4 - dc);
		});
		mc.getDsp(0).setInputProvider([this](int32_t& _l, int32_t& _r) { m_bridge.pullInput(_l, _r); });

		char buf[160];
		std::snprintf(buf, sizeof(buf), "plugin at %.0f Hz, latency %zu frames (%.1f ms), %+.0f dB",
			m_rate, m_latency, 1000.0 * static_cast<double>(m_latency) / m_rate, static_cast<double>(_gainDb));
		m_stats.audio = buf;
		m_stats.midi = "the DAW track";

		m_thread = std::thread([this] { run(); });
	}

	Runner::~Runner()
	{
		m_quit = true;
		m_thread.join();
		// The DSPs may still be finishing their last catch-up, which ends in these callbacks.
		auto& mc = m_engine.mc();
		mc.syncDsps();
		mc.getDsp(3).setBlockCallback({});
		mc.getDsp(0).setInputProvider({});
		mc.setIdle(false);
	}

	void Runner::setGainDb(const float _gainDb)
	{
		m_bridge.setGain(std::pow(10.0f, _gainDb / 20.0f));
	}

	void Runner::queueMidi(const uint32_t _offset, const uint8_t* _data, const size_t _size)
	{
		if(_size == 0 || _size > 3 || _data[0] == 0xf0)
			return;
		Event e;
		e.frame = m_consumed.load(std::memory_order_relaxed) + m_latency + _offset;
		e.size = static_cast<uint8_t>(_size);
		std::copy_n(_data, _size, e.bytes);
		if(!m_events.push(e))
			++m_dropped;
	}

	void Runner::process(float* const* _outs, const size_t _numOuts, const float* const* _ins, const size_t _numIns, const size_t _frames, const bool _offline)
	{
		size_t done = 0;
		while(done < _frames)
		{
			// Never more than the emulator is allowed to be ahead, or it could never be there.
			const size_t n = std::min(_frames - done, m_ahead);
			if(_ins && _numIns)
				m_bridge.pushInputs(_ins, _numIns, done, n);

			// Offline there is no deadline: wait for the worker, which is already allowed to be
			// at least this far. Bounded all the same, in case it has died.
			if(_offline)
			{
				const auto giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(10);
				while(m_bridge.queued() < m_debt + n && std::chrono::steady_clock::now() < giveUp)
					std::this_thread::sleep_for(std::chrono::microseconds(50));
			}

			if(m_debt)
				m_debt -= m_bridge.skip(m_debt);
			const size_t got = m_debt ? 0 : m_bridge.pull(_outs, _numOuts, done, n);
			if(got < n)
			{
				for(size_t c = 0; c < _numOuts; ++c)
					if(_outs[c])
						std::fill(_outs[c] + done + got, _outs[c] + done + n, 0.0f);
				m_debt += n - got;
				if(m_primed)
					++m_xruns;
			}
			else
				m_primed = true;

			// Only now may the worker go further, so the events queued for this block are all in.
			m_consumed.store(m_consumed.load(std::memory_order_relaxed) + n, std::memory_order_release);
			done += n;
		}
		for(size_t c = AudioBridge::Outs; c < _numOuts; ++c)
			if(_outs[c])
				std::fill(_outs[c], _outs[c] + _frames, 0.0f);
	}

	void Runner::run()
	{
		auto& mc = m_engine.mc();
		using clock = std::chrono::steady_clock;
		auto lastStats = clock::now();
		uint64_t lastCycles = mc.ucCycles();
		double busy = 0;
		std::vector<uint8_t> out, pcOut, toEditor, fromKeeper, linkIn;
		std::vector<std::vector<uint8_t>> pcIn;
		auto nowMs = [&mc] { return mc.ucCycles() / (g1::g_ucClock / 1000); };

		while(!m_quit.load(std::memory_order_acquire))
		{
			const auto t0 = clock::now();
			const uint64_t target = m_consumed.load(std::memory_order_acquire) + m_ahead;
			uint64_t produced = m_bridge.produced();

			// The events whose frame has come. An event is queued for consumed + latency + offset,
			// and the worker never gets past consumed + ahead plus one sync, which is less: so no
			// event is ever behind the emulator, and each one reaches the G1 at the first sync at
			// or after its frame, wherever the host's blocks begin and end.
			std::unique_lock<std::mutex> lock(m_engineMutex);
			// The editor's bytes, whenever they come: the PC Port has no timing to keep.
			if(m_pcPort)
			{
				m_pcPort->poll(pcIn);
				if(static_cast<size_t>(m_pcIndex) < pcIn.size() && !pcIn[static_cast<size_t>(m_pcIndex)].empty())
				{
					auto& bytes = pcIn[static_cast<size_t>(m_pcIndex)];
					m_pcIn += bytes.size();
					mc.getPcPort().receive(bytes);
					if(m_keeper)
						m_keeper->editorSent(bytes, nowMs());
					bytes.clear();
				}
			}
			// The editor on the direct link (NME, with no MIDI port: the way in on Windows) talks to the
			// same PC Port.
			if(m_link)
			{
				linkIn.clear();
				m_link->poll(linkIn);
				if(!linkIn.empty())
				{
					m_pcIn += linkIn.size();
					mc.getPcPort().receive(linkIn);
					if(m_keeper)
						m_keeper->editorSent(linkIn, nowMs());
				}
			}
			// The keeper's own requests, when the editor leaves it room.
			if(m_keeper)
			{
				fromKeeper.clear();
				m_keeper->tick(nowMs(), fromKeeper);
				if(!fromKeeper.empty())
					mc.getPcPort().receive(fromKeeper);
			}
			while(const auto* e = m_events.front())
			{
				if(e->frame > produced)
					break;
				for(uint8_t i = 0; i < e->size; ++i)
					mc.getSci().write(e->bytes[i]);
				m_midiIn += e->size;
				m_events.pop();
			}

			if(produced >= target)
			{
				lock.unlock();
				mc.setIdle(true);	// the DSP threads sleep too instead of spinning
				std::this_thread::sleep_for(std::chrono::microseconds(200));
				mc.setIdle(false);
			}
			else
			{
				uint64_t limit = target;
				if(const auto* e = m_events.front())
					limit = std::min(limit, e->frame);
				// A slice of at most 0.5 ms of G1 time, so the lock is let go often.
				const auto stop = mc.ucCycles() + g1::g_ucClock / 2000;
				while(produced < limit && mc.ucCycles() < stop)
				{
					mc.exec();
					produced = m_bridge.produced();
				}
				// The G1's MIDI OUT goes nowhere yet; its PC Port goes to the editor, if there is one.
				out.clear();
				mc.getSci().read(out);
				m_midiOut += out.size();
				pcOut.clear();
				mc.getPcPort().takeTx(pcOut);
				m_pcOut += pcOut.size();
				// The keeper sees all of it and keeps the answers to its own requests from the editor.
				if(m_keeper)
				{
					toEditor.clear();
					m_keeper->g1Sent(pcOut, nowMs(), toEditor);
					pcOut.swap(toEditor);
				}
				lock.unlock();
				if(m_pcPort && !pcOut.empty())
					m_pcPort->send(m_pcIndex, pcOut);
				if(m_link && !pcOut.empty())
					m_link->send(pcOut);
				busy += std::chrono::duration<double>(clock::now() - t0).count();
			}

			const auto t1 = clock::now();
			if(t1 - lastStats >= std::chrono::milliseconds(500))
			{
				const double wall = std::chrono::duration<double>(t1 - lastStats).count();
				const auto cycles = mc.ucCycles();
				std::lock_guard<std::mutex> statsLock(m_statsMutex);
				m_stats.seconds += wall;
				m_stats.speed = 100.0 * static_cast<double>(cycles - lastCycles) / g1::g_ucClock / wall;
				m_stats.load = 100.0 * busy / wall;
				for(uint32_t d = 0; d < g1::g_dspCount; ++d)
				{
					m_stats.dspOn[d] = mc.getDsp(d).booted();
					m_stats.dspFailed[d] = mc.getDsp(d).jitFailed();
					if(mc.getDsp(d).jitFailed() && m_stats.dspProblem.empty())
						m_stats.dspProblem = "DSP " + std::to_string(d) + " stopped: the JIT could not generate its code (" + mc.getDsp(d).jitFailure() + ")";
				}
				lastCycles = cycles;
				lastStats = t1;
				busy = 0;
			}
		}
	}

	HostStats Runner::stats()
	{
		std::lock_guard<std::mutex> lock(m_statsMutex);
		auto s = m_stats;
		s.midiIn = m_midiIn;
		s.midiOut = m_midiOut;
		s.pcIn = m_pcIn;
		s.pcOut = m_pcOut;
		s.xruns = m_xruns;
		s.peak = m_bridge.peak();
		return s;
	}
}
