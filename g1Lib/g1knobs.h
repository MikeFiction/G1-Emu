#pragma once

// What each of the 18 panel knobs is assigned to, read from the OS's own tables in RAM: the
// module and parameter names the OS shows on its display in Edit mode, the value and its range.
// Everything comes from the running OS (built from the user's ROM at boot); nothing is copied.
// Where each table lives: NOTES.md, "The knob assignments". Reading RAM from another thread is
// harmless here: at worst one refresh shows a half-updated entry.

#include "g1mc.h"

#include <array>
#include <cstdint>
#include <string>

namespace g1
{
	struct KnobInfo
	{
		bool assigned = false;
		uint8_t slot = 0;			// 0-3 = A-D
		uint8_t section = 0;		// 0 = common, 1 = poly, 2 = morph
		uint8_t module = 0;			// module index in the patch (from 1)
		uint8_t param = 0;
		uint8_t type = 0;			// module type (7 = OscA...)
		uint8_t value = 0;			// current value, 0..max (not for morph)
		uint8_t max = 0;
		std::string moduleName;		// the name given in the patch ("Mod"), or "Morph"
		std::string paramName;		// the OS's short name ("Freq coars")
	};

	class KnobMap
	{
	public:
		static constexpr uint32_t SlotBase = 0x1ab988;		// slot A; each next slot $6000 further
		static constexpr uint32_t SlotStride = 0x6000;
		static constexpr uint32_t KnobTable = 0x5c36;		// per slot: 23 x {section, module, param, -}
		static constexpr uint32_t CommonModules = 0x467a;	// per slot: a pointer per module index
		static constexpr uint32_t PolyModules = 0x4c66;
		static constexpr uint32_t ModuleType = 0x0e, ModuleName = 0x13, ModuleParams = 0x25;	// in a module
		static constexpr uint32_t ParamStride = 8, ParamValue = 3;
		static constexpr uint32_t TypeTable = 0x1c3b1c;		// per type, 48 bytes; first long = parameter names
		static constexpr uint32_t TypeStride = 48;
		static constexpr uint32_t NameStride = 12;			// 11 characters, then the maximum value
		static constexpr uint32_t MorphName = 0x15bdd8, MorphParams = 0x15bdde, MorphStride = 8;
		static constexpr uint32_t ActiveSlot = 0x1c3abe;
		static constexpr uint32_t PanelSplitOff = 0x18c0e4;	// 0 = split on
		static constexpr uint32_t SplitSlot = 0x145a94, SplitKnob = 0x145aa6;	// per panel knob, when split

		// Knobs 1-18: their ADC multiplexer channel (Microcontroller::setAdc). Checked by assigning
		// a knob to each module and moving each channel: the OS tells the editor which knob moved.
		static constexpr std::array<uint8_t, 18> KnobAdc = {0x31, 0x37, 0x2d, 0x32, 0x28, 0x2e, 0x33, 0x29, 0x2f, 0x34, 0x2a, 0x1a, 0x35, 0x2b, 0x1b, 0x36, 0x2c, 0x1c};

		explicit KnobMap(Microcontroller& _mc) : m_mc(_mc) {}

		// The addresses above are the ROM's factory OS. Clavia's 3.03b update has the same tables
		// linked $20 higher (NOTES.md, "The official OS update"). Which one runs is told by where
		// the DSP host-port table is ($200000, $200008, ...): $15BD68 or $15BD88.
		uint32_t osShift()
		{
			const auto ports = [this](const uint32_t _a) { return read32(_a) == 0x200000 && read32(_a + 4) == 0x200008; };
			return ports(0x15bd88) && !ports(0x15bd68) ? 0x20u : 0u;
		}

		// Panel knob 0-17.
		KnobInfo read(uint32_t _knob)
		{
			KnobInfo k;
			if(_knob >= 18)
				return k;
			uint32_t knob = _knob;
			const uint32_t os = osShift();
			k.slot = static_cast<uint8_t>(m_mc.read8(ActiveSlot + os) & 3);
			if(m_mc.read8(PanelSplitOff + os) == 0)
			{
				k.slot = static_cast<uint8_t>(m_mc.read8(SplitSlot + os + _knob) & 3);
				knob = m_mc.read8(SplitKnob + os + _knob);
				if(knob >= 18)
					return k;
			}
			const uint32_t base = SlotBase + os + k.slot * SlotStride;
			const uint32_t entry = base + KnobTable + knob * 4;
			k.section = m_mc.read8(entry);
			k.module = m_mc.read8(entry + 1);
			k.param = m_mc.read8(entry + 2);

			if(k.section == 2)
			{
				// A morph group: four of them, named by the OS
				if(k.param >= 4)
					return k;
				k.moduleName = readString(MorphName + os, 6);
				k.paramName = readString(MorphParams + os + k.param * MorphStride, MorphStride);
				k.assigned = !k.paramName.empty();
				return k;
			}
			if(k.section > 1 || k.module == 0 || k.module >= 128)
				return k;
			const uint32_t mod = read32(base + (k.section ? PolyModules : CommonModules) + k.module * 4);
			if(!inRam(mod))
				return k;
			k.type = m_mc.read8(mod + ModuleType);
			const uint32_t names = read32(TypeTable + os + k.type * TypeStride);
			if(!inRam(names))
				return k;
			const uint32_t name = names + k.param * NameStride;
			k.paramName = readString(name, NameStride - 1);
			k.max = m_mc.read8(name + NameStride - 1);
			k.moduleName = readString(mod + ModuleName, 16);
			k.value = m_mc.read8(mod + ModuleParams + k.param * ParamStride + ParamValue);
			k.assigned = !k.paramName.empty();
			return k;
		}

		// The knob position (ADC value) that gives _value on a parameter that goes up to _max: the
		// OS takes value = position * (max + 1) / 256, and ignores 0 and 255 as positions.
		static uint8_t positionFor(const uint8_t _value, const uint8_t _max)
		{
			const uint32_t pos = ((2u * _value + 1u) * 128u) / (static_cast<uint32_t>(_max) + 1u);
			return static_cast<uint8_t>(pos < 1 ? 1 : (pos > 254 ? 254 : pos));
		}

	private:
		static bool inRam(const uint32_t _a) { return _a >= 0x100000 && _a < 0x200000; }

		uint32_t read32(const uint32_t _a)
		{
			return (static_cast<uint32_t>(m_mc.read8(_a)) << 24) | (static_cast<uint32_t>(m_mc.read8(_a + 1)) << 16)
				| (static_cast<uint32_t>(m_mc.read8(_a + 2)) << 8) | m_mc.read8(_a + 3);
		}

		std::string readString(const uint32_t _a, const uint32_t _max)
		{
			std::string s;
			for(uint32_t i = 0; i < _max; ++i)
			{
				const auto c = static_cast<char>(m_mc.read8(_a + i));
				if(c == 0)
					break;
				s += (c >= 32 && c < 127) ? c : ' ';
			}
			while(!s.empty() && s.back() == ' ')
				s.pop_back();
			return s;
		}

		Microcontroller& m_mc;
	};
}
