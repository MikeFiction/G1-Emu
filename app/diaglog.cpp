#include "diaglog.h"

#include "g1Lib/g1mc.h"
#include "hostlog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#define G1_HAVE_CPUID 1
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#define G1_HAVE_CPUID 1
#endif

namespace g1app
{
	namespace
	{
#ifdef G1_HAVE_CPUID
		void cpuid(const uint32_t _leaf, const uint32_t _sub, uint32_t _r[4])
		{
#ifdef _MSC_VER
			int r[4];
			__cpuidex(r, static_cast<int>(_leaf), static_cast<int>(_sub));
			for(int i = 0; i < 4; ++i)
				_r[i] = static_cast<uint32_t>(r[i]);
#else
			__cpuid_count(_leaf, _sub, _r[0], _r[1], _r[2], _r[3]);
#endif
		}

		std::string cpuName()
		{
			uint32_t r[4];
			cpuid(0x80000000u, 0, r);
			if(r[0] < 0x80000004u)
				return "unknown x86 CPU";
			char name[49] = {};
			for(uint32_t i = 0; i < 3; ++i)
			{
				cpuid(0x80000002u + i, 0, r);
				std::memcpy(name + 16 * i, r, 16);
			}
			std::string s(name);
			s.erase(0, s.find_first_not_of(' '));
			s.erase(s.find_last_not_of(' ') + 1);
			return s;
		}

		// The ones the DSP JIT chooses code by (jitemitter.cpp, hasBMI/hasBMI2), and the wider
		// ones, in case a library picks a path by them.
		std::string cpuFeatures()
		{
			uint32_t r[4];
			cpuid(0, 0, r);
			if(r[0] < 7)
				return "no leaf 7";
			cpuid(7, 0, r);
			const uint32_t ebx = r[1];
			std::string s;
			const auto add = [&](const bool _on, const char* _name) { if(_on) s += s.empty() ? _name : std::string(" ") + _name; };
			add(ebx & (1u << 3), "BMI");
			add(ebx & (1u << 8), "BMI2");
			add(ebx & (1u << 5), "AVX2");
			add(ebx & (1u << 16), "AVX512F");
			return s.empty() ? "none of BMI/BMI2/AVX2/AVX512F" : s;
		}
#else
		std::string cpuName() { return "non-x86 CPU"; }
		std::string cpuFeatures() { return "n/a"; }
#endif

		// "8 performance + 4 efficiency cores" on a hybrid CPU, empty otherwise or when unknown.
		std::string hybridCores()
		{
#if defined(_WIN32)
			DWORD len = 0;
			GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
			if(len == 0)
				return {};
			std::vector<uint8_t> buf(len);
			auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data());
			if(!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len))
				return {};
			std::array<uint32_t, 256> perClass{};
			uint32_t maxClass = 0;
			for(DWORD off = 0; off < len;)
			{
				auto* p = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data() + off);
				const auto c = p->Processor.EfficiencyClass;
				++perClass[c];
				maxClass = std::max<uint32_t>(maxClass, c);
				off += p->Size;
			}
			if(maxClass == 0)
				return {};
			uint32_t efficiency = 0;
			for(uint32_t c = 0; c < maxClass; ++c)
				efficiency += perClass[c];
			return std::to_string(perClass[maxClass]) + " performance + " + std::to_string(efficiency) + " efficiency cores";
#elif defined(__linux__)
			// Intel's hybrid CPUs show two PMUs: cpu_core and cpu_atom, each with its CPU list.
			const auto count = [](const char* _path)
			{
				std::ifstream f(_path);
				std::string list;
				if(!(f >> list))
					return 0;
				int n = 0;
				size_t pos = 0;
				while(pos < list.size())
				{
					const auto comma = list.find(',', pos);
					const auto item = list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
					const auto dash = item.find('-');
					n += dash == std::string::npos ? 1 : std::stoi(item.substr(dash + 1)) - std::stoi(item.substr(0, dash)) + 1;
					pos = comma == std::string::npos ? list.size() : comma + 1;
				}
				return n;
			};
			const int p = count("/sys/devices/cpu_core/cpus"), e = count("/sys/devices/cpu_atom/cpus");
			if(p == 0 || e == 0)
				return {};
			return std::to_string(p) + " performance + " + std::to_string(e) + " efficiency threads";
#else
			return {};
#endif
		}

		// 24-bit full scale, as the DSPs see it.
		std::string dbfs(const double _v)
		{
			if(_v < 1.0)
				return "silent";
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f dBFS", 20.0 * std::log10(_v / 8388608.0));
			return buf;
		}
	}

	std::string systemSummary(const bool _dspThreads)
	{
		std::string s = "system: " + cpuName() + ", " + std::to_string(std::thread::hardware_concurrency()) + " threads";
		if(const auto h = hybridCores(); !h.empty())
			s += " (hybrid: " + h + ")";
		s += ", " + cpuFeatures();
		s += _dspThreads ? ", DSPs on their own threads" : ", DSPs serial (G1_THREADS=0)";
		return s;
	}

	void HealthProbe::midiIn(g1::Microcontroller& _mc, const uint64_t _nowMs)
	{
		if(m_pending || (m_any && _nowMs - m_lastMs < 20000))
			return;
		// From here on: the peaks and the misses are the ones of this window only. Synced first:
		// the DSP threads write the link peaks.
		_mc.syncDsps();
		for(uint32_t d = 0; d < g1::g_dspCount; ++d)
		{
			_mc.getDsp(d).resetLinkPeak();
			m_misses[d] = _mc.getDsp(d).linkMisses();
		}
		m_peak.store(0, std::memory_order_relaxed);
		m_pending = true;
		m_dueMs = _nowMs + 1500;
	}

	void HealthProbe::tick(g1::Microcontroller& _mc, const uint64_t _nowMs)
	{
		if(!m_pending || _nowMs < m_dueMs)
			return;
		_mc.syncDsps();
		// Each DSP's sound goes to the next through its link; DSP 3's is the output itself.
		std::string s = "health, 1.5 s after MIDI in: output 1/2 " + dbfs(m_peak.load(std::memory_order_relaxed)) + " (before the gain)";
		for(uint32_t d = 0; d < g1::g_dspCount; ++d)
		{
			auto& dsp = _mc.getDsp(d);
			s += "\n  DSP " + std::to_string(d) + ": ";
			s += dsp.jitFailed() ? "JIT failed" : dsp.booted() ? "running" : "not booted";
			if(dsp.jitRecoveries())
				s += ", " + std::to_string(dsp.jitRecoveries()) + " JIT recoveries";
			if(d + 1 < g1::g_dspCount)
			{
				const auto& p = dsp.linkPeak();
				s += ", link to DSP " + std::to_string(d + 1) + " " + dbfs(*std::max_element(p.begin(), p.end()));
			}
			if(d > 0)
				s += ", link misses " + std::to_string(dsp.linkMisses() - m_misses[d]);
		}
		logEvent(s);
		m_pending = false;
		m_any = true;
		m_lastMs = _nowMs;
	}
}
