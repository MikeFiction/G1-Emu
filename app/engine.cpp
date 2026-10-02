#include "engine.h"

#include <algorithm>

namespace g1app
{
	namespace
	{
		// "G1US", then the version, the factory hash and the number of runs; each run is its
		// offset, its length and its bytes. All little endian.
		constexpr uint32_t g_magic = 0x53553147;
		constexpr uint32_t g_version = 1;
		// Two differences closer than this go in one run: fewer headers, and the few bytes in
		// between are the flash's contents at that point anyway.
		constexpr size_t g_joinGap = 16;

		uint32_t fnv1a(const std::vector<uint8_t>& _data)
		{
			uint32_t h = 0x811c9dc5;
			for(const auto b : _data)
				h = (h ^ b) * 0x01000193;
			return h;
		}

		void put32(std::vector<uint8_t>& _out, const uint32_t _v)
		{
			for(int i = 0; i < 4; ++i)
				_out.push_back(static_cast<uint8_t>(_v >> (8 * i)));
		}

		bool get32(const std::vector<uint8_t>& _in, size_t& _pos, uint32_t& _v)
		{
			if(_pos > _in.size() || _in.size() - _pos < 4)
				return false;
			_v = 0;
			for(int i = 0; i < 4; ++i)
				_v |= static_cast<uint32_t>(_in[_pos + static_cast<size_t>(i)]) << (8 * i);
			_pos += 4;
			return true;
		}
	}

	Engine::Engine(const std::vector<uint8_t>& _rom) : m_mc(std::make_unique<g1::Microcontroller>(_rom))
	{
		m_mc->installRomOsInFlash();
		m_factory = m_mc->getFlash().data();
		m_factoryHash = fnv1a(m_factory);
	}

	Engine::~Engine() = default;

	bool Engine::loadFlash(const std::vector<uint8_t>& _image)
	{
		if(_image.size() != g1::Flash::Size)
			return false;
		m_mc->getFlash().data() = _image;
		return true;
	}

	std::vector<uint8_t> Engine::userState() const
	{
		const auto& flash = m_mc->getFlash().data();
		std::vector<uint8_t> runs;
		uint32_t count = 0;
		size_t i = 0;
		while(i < flash.size())
		{
			if(flash[i] == m_factory[i])
			{
				++i;
				continue;
			}
			const size_t begin = i;
			size_t last = i;	// the last byte that differs
			for(size_t j = i + 1; j < flash.size() && j - last <= g_joinGap; ++j)
				if(flash[j] != m_factory[j])
					last = j;
			put32(runs, static_cast<uint32_t>(begin));
			put32(runs, static_cast<uint32_t>(last + 1 - begin));
			runs.insert(runs.end(), flash.begin() + static_cast<std::ptrdiff_t>(begin), flash.begin() + static_cast<std::ptrdiff_t>(last + 1));
			++count;
			i = last + 1;
		}

		std::vector<uint8_t> out;
		out.reserve(16 + runs.size());
		put32(out, g_magic);
		put32(out, g_version);
		put32(out, m_factoryHash);
		put32(out, count);
		out.insert(out.end(), runs.begin(), runs.end());
		return out;
	}

	bool Engine::setUserState(const std::vector<uint8_t>& _state, std::string& _error)
	{
		size_t pos = 0;
		uint32_t magic = 0, version = 0, hash = 0, count = 0;
		if(!get32(_state, pos, magic) || magic != g_magic || !get32(_state, pos, version) || !get32(_state, pos, hash) || !get32(_state, pos, count))
		{
			_error = "not a G1-Emu state";
			return false;
		}
		if(version != g_version)
		{
			_error = "made by a newer G1-Emu (state version " + std::to_string(version) + ")";
			return false;
		}
		if(hash != m_factoryHash)
		{
			_error = "made with another ROM or OS version";
			return false;
		}

		// Checked in full before anything is written, so a damaged state changes nothing.
		auto flash = m_factory;
		for(uint32_t r = 0; r < count; ++r)
		{
			uint32_t offset = 0, length = 0;
			if(!get32(_state, pos, offset) || !get32(_state, pos, length) || offset > flash.size() || length > flash.size() - offset || length > _state.size() - pos)
			{
				_error = "damaged state";
				return false;
			}
			std::copy_n(_state.begin() + static_cast<std::ptrdiff_t>(pos), length, flash.begin() + offset);
			pos += length;
		}
		m_mc->getFlash().data() = std::move(flash);
		return true;
	}
}
