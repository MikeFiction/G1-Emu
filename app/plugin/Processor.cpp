#include "Processor.h"

#include "Editor.h"

#include "romfinder.h"
#include "g1Lib/g1format.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <utility>

namespace g1plugin
{
	namespace
	{
		constexpr const char* g_stateTag = "G1EmuState";
		constexpr int g_stateVersion = 1;

		juce::String packBytes(const std::vector<uint8_t>& _data)
		{
			juce::MemoryOutputStream packed;
			{
				juce::GZIPCompressorOutputStream zip(packed, 9);
				zip.write(_data.data(), _data.size());
			}
			return packed.getMemoryBlock().toBase64Encoding();
		}

		std::vector<uint8_t> toBytes(const juce::MemoryBlock& _block)
		{
			const auto* p = static_cast<const uint8_t*>(_block.getData());
			return std::vector<uint8_t>(p, p + _block.getSize());
		}

		bool unpackBytes(const juce::String& _text, std::vector<uint8_t>& _data)
		{
			juce::MemoryBlock packed;
			if(!packed.fromBase64Encoding(_text))
				return false;
			juce::MemoryInputStream in(packed, false);
			juce::GZIPDecompressorInputStream zip(in);
			juce::MemoryBlock plain;
			juce::MemoryOutputStream out(plain, false);
			out.writeFromInputStream(zip, -1);
			out.flush();
			_data = toBytes(plain);
			return true;
		}

		// The numbers of the live instances, so each PC Port gets a name of its own and a freed
		// number is reused (close the second of three and the next one is 2 again). A host can put
		// each instance in a process of its own (Bitwig does), so a number is held with a lock
		// every process sees (a file lock: the system lets it go if the process dies), kept for
		// the instance's life. JUCE's list of MIDI ports is no use for this: another process's
		// new port reaches it late. It is checked as well, for ports made some other way.
		std::mutex g_instancesMutex;
		std::set<int> g_instances;

		std::string pcPortClient(const int _n)
		{
			return _n == 1 ? std::string("G1-Emu") : "G1-Emu " + std::to_string(_n);
		}

		int takeInstanceNumber(std::unique_ptr<juce::InterProcessLock>& _hold)
		{
			std::set<juce::String> taken;
			for(const auto& d : juce::MidiInput::getAvailableDevices())
				taken.insert(d.name);
			for(const auto& d : juce::MidiOutput::getAvailableDevices())
				taken.insert(d.name);
			std::lock_guard<std::mutex> lock(g_instancesMutex);
			for(int n = 1;; ++n)
			{
				if(g_instances.count(n) || taken.count(juce::String(pcPortClient(n) + " PC Port")))
					continue;
				auto hold = std::make_unique<juce::InterProcessLock>("G1-Emu PC Port " + juce::String(n));
				if(!hold->enter(0))
					continue;
				_hold = std::move(hold);
				g_instances.insert(n);
				return n;
			}
		}

		void releaseInstanceNumber(const int _n)
		{
			std::lock_guard<std::mutex> lock(g_instancesMutex);
			g_instances.erase(_n);
		}
	}

	// ____________________________________________________________________________________________
	// The knobs as parameters

	KnobParameter::KnobParameter(const int _index)
		: juce::AudioParameterFloat(juce::ParameterID{"knob" + juce::String(_index + 1), 1}, "Knob " + juce::String(_index + 1),
			juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f / 253.0f), 0.0f),	// the knob's 254 positions
		  m_index(_index), m_name("Knob " + juce::String(_index + 1))
	{
	}

	juce::String KnobParameter::getName(const int _maxLength) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_name.substring(0, _maxLength);
	}

	bool KnobParameter::setInfo(const g1::KnobInfo& _info)
	{
		juce::String name = "Knob " + juce::String(m_index + 1);
		if(_info.assigned)
			name << ": " << juce::String(_info.moduleName) << " " << juce::String(_info.paramName);
		std::lock_guard<std::mutex> lock(m_mutex);
		m_info = _info;
		if(name == m_name)
			return false;
		m_name = name;
		return true;
	}

	juce::String KnobParameter::getText(const float _v, const int _maxLength) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		// value = position * (max + 1) / 256, as the OS takes it (g1knobs.h, positionFor), read
		// as the editor shows it ("Sine", "1.25 kHz": g1format.h)
		if(m_info.assigned && m_info.section != 2 && m_info.max > 0)
			return juce::String(g1::formatValue(m_info.type, m_info.param, toAdc(_v) * (m_info.max + 1) / 256)).substring(0, _maxLength);
		return juce::String(juce::roundToInt(_v * 100.0f)) + "%";
	}

	// The way back of getText: a percentage, the text of one of the values ("Saw"), or the OS's
	// number for the value of the parameter the knob moves.
	float KnobParameter::getValueForText(const juce::String& _text) const
	{
		const auto t = _text.trim();
		const float n = t.getFloatValue();
		std::lock_guard<std::mutex> lock(m_mutex);
		if(t.endsWithChar('%') || !m_info.assigned || m_info.section == 2 || m_info.max == 0)
			return juce::jlimit(0.0f, 1.0f, n / 100.0f);
		for(int v = 0; v <= m_info.max; ++v)
			if(t.equalsIgnoreCase(juce::String(g1::formatValue(m_info.type, m_info.param, v))))
				return toParam(g1::KnobMap::positionFor(static_cast<uint8_t>(v), m_info.max));
		const auto value = static_cast<uint8_t>(juce::jlimit(0, static_cast<int>(m_info.max), juce::roundToInt(n)));
		return toParam(g1::KnobMap::positionFor(value, m_info.max));
	}

	Processor::Processor()
		: juce::AudioProcessor(BusesProperties()
			.withInput("In L/R", juce::AudioChannelSet::stereo(), false)
			.withOutput("Out 1/2", juce::AudioChannelSet::stereo(), true)
			.withOutput("Out 3/4", juce::AudioChannelSet::stereo(), true))
	{
		for(int k = 0; k < 18; ++k)
		{
			auto param = std::make_unique<KnobParameter>(k);
			m_knobParams[static_cast<size_t>(k)] = param.get();
			addParameter(param.release());
		}
		auto volume = std::make_unique<VolumeParameter>();
		m_volumeParam = volume.get();
		addParameter(volume.release());
		loadPreferences();
		openPcPort();
		openLink();
		findRom();
		startTimerHz(20);
	}

	namespace
	{
		std::string preferencesPath()
		{
			return (std::filesystem::path(g1app::defaultSettingsPath()).parent_path() / "plugin.conf").string();
		}
	}

	// What the last editor was left like: a new instance starts there, and a project's state, when
	// one comes, has the last word. Plain "key = value", like the settings file.
	void Processor::loadPreferences()
	{
		std::ifstream f(preferencesPath());
		std::string line;
		while(std::getline(f, line))
		{
			const auto eq = line.find('=');
			if(eq == std::string::npos || line[0] == '#')
				continue;
			const auto key = juce::String(line.substr(0, eq)).trim();
			const auto value = juce::String(line.substr(eq + 1)).trim();
			if(key == "extrasOpen")				m_extrasOpen = value != "0";
			else if(key == "knobDisplays")		m_knobDisplays = value != "0";
			else if(key == "knobFollowsPatch")	m_knobFollowsPatch = value != "0";
			else if(key == "randomExclude")		m_randomExclude = g1app::knobListFromString(value.toStdString());
			else if(key == "panelScale")		m_panelScale = juce::jlimit(g1gui::PanelView::MinScale, g1gui::PanelView::MaxScale, value.getFloatValue());
		}
	}

	void Processor::savePreferences() const
	{
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(preferencesPath()).parent_path(), ec);
		std::ofstream f(preferencesPath(), std::ios::trunc);
		f << "# G1-Emu plugin: how a new instance's window starts (a project keeps its own).\n"
		  << "extrasOpen = " << (m_extrasOpen ? 1 : 0) << "\n"
		  << "knobDisplays = " << (m_knobDisplays ? 1 : 0) << "\n"
		  << "knobFollowsPatch = " << (m_knobFollowsPatch ? 1 : 0) << "\n"
		  << "randomExclude = " << g1app::knobListToString(m_randomExclude) << "\n"
		  << "panelScale = " << m_panelScale << "\n";
	}

	Processor::~Processor()
	{
		stopTimer();
		cancelPendingUpdate();
		{
			std::lock_guard<std::mutex> lock(m_lifecycle);
			m_runner.reset();
			m_engine.reset();
		}
		m_pcPort.reset();
		m_link.stop();
		if(m_instance > 0)
			releaseInstanceNumber(m_instance);
		m_instanceLock.reset();
	}

	// The G1's PC Port as a virtual MIDI port, for the editor (#8 will add a direct link beside it).
	// G1_PLUGIN_PC_PORT=0 leaves it out.
	void Processor::openPcPort()
	{
		if(const char* v = std::getenv("G1_PLUGIN_PC_PORT"); v && *v == '0')
		{
			m_pcProblem = "off (G1_PLUGIN_PC_PORT=0)";
			return;
		}
		// JUCE's MIDI endpoints are a singleton that JUCE 8 never makes again once it has shut
		// down, and the CLAP wrapper shuts JUCE down with its last instance: the next instance in
		// the same process finds none, and would crash on it. (Holding JUCE up from here is worse:
		// it outlives the module when the host unloads it.)
		if(juce::ump::Endpoints::getInstance() == nullptr)
		{
			m_pcProblem = "none in this instance: JUCE's MIDI was shut down with the last instance in this "
				"process (CLAP). Reload the plugin, or keep one instance open, to have it";
			return;
		}
		m_instance = takeInstanceNumber(m_instanceLock);
		m_pcPort = std::make_unique<g1app::JuceMidi>(pcPortClient(m_instance).c_str());
		m_pcIndex = m_pcPort->addPort("PC Port");
	}

	void Processor::openLink()
	{
		if(const char* v = std::getenv("G1_DIRECT_LINK"); v && *v == '0')
		{
			m_linkProblem = "off (G1_DIRECT_LINK=0)";
			return;
		}
		g1app::HostOptions options;
		options.load(g1app::defaultSettingsPath());
		if(!options.directLink)
		{
			m_linkProblem = "off (directLink = 0 in the settings)";
			return;
		}
		if(!m_link.start("G1-Emu plugin", m_linkProblem))
			return;
		// Numbered by the port it got, which is what tells instances apart in the editor.
		const int n = m_link.port() - g1app::DirectLink::kBasePort;
		m_link.setName("G1-Emu plugin " + std::to_string(n + 1));
		if(m_pcPort && m_pcPort->virtualPorts())
		{
			m_link.setPcPortIds(m_pcPort->portIds(m_pcIndex));
			m_link.setPcPortName(m_pcPort->portListName(m_pcIndex));
		}
	}

	void Processor::findRom()
	{
		g1app::HostOptions options;
		options.load(g1app::defaultSettingsPath());
		if(const char* v = std::getenv("G1_ROM"))
			options.rom = v;
		const auto search = g1app::findRom({}, options.rom);
		std::vector<uint8_t> rom;
		if(search.found() && g1app::inspectRom(search.path, rom).ok())
		{
			std::string osNote;
			m_os = options.loadOs(osNote);
			m_rom = std::move(rom);
			m_romPath = search.path;
			m_romProblem.clear();
		}
		else
			m_romProblem = g1app::missingRomMessage(search);
	}

	void Processor::useRom(const juce::File& _file)
	{
		std::vector<uint8_t> rom;
		const auto check = g1app::inspectRom(_file.getFullPathName().toStdString(), rom);
		if(!check.ok())
		{
			m_romProblem = _file.getFullPathName().toStdString() + "\n    " + check.what() + "\n\n" + m_romProblem;
			++m_generation;		// the editor shows the new message
			return;
		}
		// Remembered for next time, in the same file and the same key the standalone uses.
		g1app::HostOptions options;
		options.load(g1app::defaultSettingsPath());
		options.rom = _file.getFullPathName().toStdString();
		options.save(g1app::defaultSettingsPath());

		std::string osNote;
		m_os = options.loadOs(osNote);
		m_rom = std::move(rom);
		m_romPath = options.rom;
		m_romProblem.clear();

		suspendProcessing(true);
		{
			std::lock_guard<std::mutex> lock(m_lifecycle);
			createEngine(m_haveState ? &m_state : nullptr);
			startRunner();
		}
		suspendProcessing(false);
	}

	// ____________________________________________________________________________________________
	// The engine's life. m_lifecycle is held by the caller.

	void Processor::createEngine(const juce::MemoryBlock* _state)
	{
		m_runner.reset();
		m_engine.reset();
		m_keeper.reset();
		if(m_rom.empty())
			return;
		auto engine = std::make_unique<g1app::Engine>(m_rom, m_os);
		m_keeper = std::make_unique<g1app::SlotKeeper>();
		m_synthSettings.reset();
		m_unstarted = false;
		if(_state)
			applyState(*engine, *_state);
		else
		{
			m_keepProjectState = false;
			m_unstarted = true;
			startFromStandalone(*engine);
		}
		m_engine = std::move(engine);
		m_engineFresh = true;
		++m_generation;
		knobsFromEngine();
	}

	// A new engine: its knobs are where its state put them, and the parameters take those
	// positions, without it counting as a gesture. Done here and not in the timer, so a host that
	// moves a knob right after (with no message loop running in between) is not overwritten.
	void Processor::knobsFromEngine()
	{
		std::lock_guard<std::mutex> lock(m_knobMutex);
		auto& mc = m_engine->mc();
		for(size_t k = 0; k < 18; ++k)
		{
			const auto adc = mc.adc(g1::KnobMap::KnobAdc[k]);
			m_lastAdc[k] = adc;
			m_knobParams[k]->setValueNotifyingHost(KnobParameter::toParam(adc));
			m_lastParam[k] = m_knobParams[k]->get();
		}
		m_lastVolumeAdc = mc.adc(g1::g_adcVolume);
		m_volumeParam->setNotifyingHost(VolumeParameter::fromAdc(m_lastVolumeAdc));
		m_lastVolumeParam = m_volumeParam->get();
		m_knobGeneration = m_generation.load();
	}

	// A new instance's banks: a copy of the standalone's flash, or the factory's.
	void Processor::startFromStandalone(g1app::Engine& _engine)
	{
		const juce::File flash(g1app::defaultFlashPath());
		juce::MemoryBlock data;
		if(flash.existsAsFile() && flash.loadFileAsData(data) && _engine.loadFlash(toBytes(data)))
			m_origin = "a copy of the standalone's flash (" + flash.getFullPathName().toStdString() + ")";
		else
			m_origin = "the factory flash: no patches yet";
	}

	void Processor::startRunner()
	{
		m_runner.reset();
		if(!m_engine || !m_prepared)
			return;
		m_runner = std::make_unique<g1app::Runner>(*m_engine, m_rate, static_cast<size_t>(m_maxBlock), m_gainDb,
			m_pcPort && m_pcPort->virtualPorts() ? m_pcPort.get() : nullptr, m_pcIndex, m_keeper.get(),
			&m_synthSettings, m_link.listening() ? &m_link : nullptr);
		setLatencySamples(static_cast<int>(m_runner->latency()));
		m_unstarted = false;
		if(std::exchange(m_engineFresh, false))
			replayPrograms();
	}

	// Queued before the host's first block, half a second into the G1's life: the OS takes them
	// even earlier (its MIDI IN holds the bytes until it reads them), this is margin.
	void Processor::replayPrograms()
	{
		const auto at = static_cast<uint32_t>(m_rate * 0.5);
		for(uint8_t ch = 0; ch < 16; ++ch)
		{
			const auto& p = m_programs[ch];
			if(p.program < 0)
				continue;
			for(const auto [cc, value] : {std::pair{uint8_t(0), p.bankMsb.load()}, std::pair{uint8_t(32), p.bankLsb.load()}})
				if(value >= 0)
				{
					const uint8_t msg[3] = {static_cast<uint8_t>(0xb0 | ch), cc, static_cast<uint8_t>(value)};
					m_runner->queueMidi(at, msg, 3);
				}
			const uint8_t msg[2] = {static_cast<uint8_t>(0xc0 | ch), static_cast<uint8_t>(p.program.load())};
			m_runner->queueMidi(at, msg, 2);
		}
	}

	// Any thread (the host's program parameter): processBlock is the one that queues MIDI.
	void Processor::setCurrentProgram(const int _index)
	{
		if(_index < 0 || _index > 127)
			return;
		m_programs[0].program.store(_index, std::memory_order_relaxed);
		m_hostProgram.store(_index, std::memory_order_release);
	}

	// "channel:msb:lsb:program" for each channel that had a Program Change, separated by spaces.
	std::string Processor::programsToString() const
	{
		std::string out;
		for(int ch = 0; ch < 16; ++ch)
		{
			const auto& p = m_programs[static_cast<size_t>(ch)];
			if(p.program >= 0)
				out += std::to_string(ch) + ":" + std::to_string(p.bankMsb.load()) + ":" + std::to_string(p.bankLsb.load()) + ":" + std::to_string(p.program.load()) + " ";
		}
		return out;
	}

	void Processor::programsFromString(const juce::String& _text)
	{
		for(auto& p : m_programs)
			p.bankMsb = p.bankLsb = p.program = -1;
		for(const auto& item : juce::StringArray::fromTokens(_text, " ", {}))
		{
			const auto f = juce::StringArray::fromTokens(item, ":", {});
			if(f.size() != 4 || f[0].getIntValue() < 0 || f[0].getIntValue() > 15)
				continue;
			auto& p = m_programs[static_cast<size_t>(f[0].getIntValue())];
			p.bankMsb = juce::jlimit(-1, 127, f[1].getIntValue());
			p.bankLsb = juce::jlimit(-1, 127, f[2].getIntValue());
			p.program = juce::jlimit(-1, 127, f[3].getIntValue());
		}
	}

	// ____________________________________________________________________________________________
	// Audio

	void Processor::prepareToPlay(const double _rate, const int _maxBlock)
	{
		std::lock_guard<std::mutex> lock(m_lifecycle);
		m_rate = _rate;
		m_maxBlock = std::max(_maxBlock, 1);
		m_prepared = true;
		m_inputs.setSize(2, m_maxBlock);
		if(!m_engine)
			createEngine(m_haveState ? &m_state : nullptr);
		startRunner();
	}

	void Processor::releaseResources()
	{
		std::lock_guard<std::mutex> lock(m_lifecycle);
		m_runner.reset();
		m_prepared = false;
	}

	bool Processor::isBusesLayoutSupported(const BusesLayout& _layouts) const
	{
		if(_layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
			return false;
		for(int i = 1; i < _layouts.outputBuses.size(); ++i)
			if(!_layouts.outputBuses[i].isDisabled() && _layouts.outputBuses[i] != juce::AudioChannelSet::stereo())
				return false;
		for(const auto& in : _layouts.inputBuses)
			if(!in.isDisabled() && in != juce::AudioChannelSet::stereo())
				return false;
		return true;
	}

	// The runner is only swapped while the host is not in here: in prepareToPlay/releaseResources,
	// or with processing suspended.
	void Processor::processBlock(juce::AudioBuffer<float>& _buffer, juce::MidiBuffer& _midi)
	{
		juce::ScopedNoDenormals noDenormals;
		auto* runner = m_runner.get();
		const auto frames = _buffer.getNumSamples();
		if(!runner || frames <= 0)
		{
			_buffer.clear();
			return;
		}

		// Inputs and outputs share the buffer's channels: the inputs are copied out first.
		const float* ins[2] = {};
		size_t numIns = 0;
		if(auto* bus = getBus(true, 0); bus && bus->isEnabled() && frames <= m_inputs.getNumSamples())
		{
			const auto in = getBusBuffer(_buffer, true, 0);
			for(int c = 0; c < 2 && c < in.getNumChannels(); ++c)
			{
				m_inputs.copyFrom(c, 0, in, c, 0, frames);
				ins[c] = m_inputs.getReadPointer(c);
				numIns = static_cast<size_t>(c) + 1;
			}
		}

		if(const int program = m_hostProgram.exchange(-1, std::memory_order_acq_rel); program >= 0)
		{
			const uint8_t msg[2] = {0xc0, static_cast<uint8_t>(program)};
			runner->queueMidi(0, msg, 2);
		}
		for(const auto m : _midi)
		{
			runner->queueMidi(static_cast<uint32_t>(m.samplePosition), m.data, static_cast<size_t>(m.numBytes));
			const auto status = m.data[0] & 0xf0;
			auto& p = m_programs[static_cast<size_t>(m.data[0] & 0x0f)];
			if(status == 0xc0 && m.numBytes >= 2)
				p.program.store(m.data[1], std::memory_order_relaxed);
			else if(status == 0xb0 && m.numBytes >= 3 && (m.data[1] == 0 || m.data[1] == 32))
				(m.data[1] == 0 ? p.bankMsb : p.bankLsb).store(m.data[2], std::memory_order_relaxed);
		}

		float* outs[4] = {};
		for(int b = 0; b < 2; ++b)
		{
			auto* bus = getBus(false, b);
			if(!bus || !bus->isEnabled())
				continue;
			auto out = getBusBuffer(_buffer, false, b);
			for(int c = 0; c < 2 && c < out.getNumChannels(); ++c)
				outs[b * 2 + c] = out.getWritePointer(c);
		}
		knobsFromHost();
		runner->process(outs, 4, ins, numIns, static_cast<size_t>(frames), isNonRealtime());

		// Channels that are an input and no output of ours.
		for(int c = getTotalNumOutputChannels(); c < _buffer.getNumChannels(); ++c)
			_buffer.clear(c, 0, frames);
	}

	// ____________________________________________________________________________________________
	// State: the G1's user state (the flash against the factory one), the knob positions, and the
	// panel's two preferences. Never the OS: Engine::userState.

	// Asked for a state before there is a G1 (the host saves before it starts the audio): what the
	// plugin has besides the flash, which applyState takes as a new instance's.
	juce::XmlElement Processor::stateXml() const
	{
		juce::XmlElement xml(g_stateTag);
		xml.setAttribute("version", g_stateVersion);
		xml.setAttribute("extrasOpen", m_extrasOpen);
		xml.setAttribute("knobDisplays", m_knobDisplays);
		xml.setAttribute("panelScale", static_cast<double>(m_panelScale));
		xml.setAttribute("knobFollowsPatch", m_knobFollowsPatch);
		xml.setAttribute("randomExclude", juce::String(g1app::knobListToString(m_randomExclude)));
		xml.setAttribute("programs", juce::String(programsToString()));
		return xml;
	}

	void Processor::readPreferences(const juce::XmlElement& _xml)
	{
		m_extrasOpen = _xml.getBoolAttribute("extrasOpen", m_extrasOpen);
		m_knobDisplays = _xml.getBoolAttribute("knobDisplays", m_knobDisplays);
		m_panelScale = static_cast<float>(_xml.getDoubleAttribute("panelScale", m_panelScale));
		m_knobFollowsPatch = _xml.getBoolAttribute("knobFollowsPatch", m_knobFollowsPatch);
		if(_xml.hasAttribute("randomExclude"))
			m_randomExclude = g1app::knobListFromString(_xml.getStringAttribute("randomExclude").toStdString());
	}

	juce::MemoryBlock Processor::settingsOnlyState()
	{
		auto xml = stateXml();
		// The knobs, as far as the host has turned them: their positions, 1..254.
		juce::StringArray knobs;
		for(auto* p : m_knobParams)
			knobs.add(juce::String(KnobParameter::toAdc(p->get())));
		xml.setAttribute("knobs", knobs.joinIntoString(" "));
		xml.setAttribute("volume", VolumeParameter::toAdc(m_volumeParam->get()));
		juce::MemoryBlock out;
		copyXmlToBinary(xml, out);
		return out;
	}

	juce::MemoryBlock Processor::snapshotState()
	{
		knobsFromHost();	// what the host turned while no audio ran (CLAP's flush) goes in too
		std::vector<uint8_t> flash, knobs(256);
		{
			std::unique_lock<std::mutex> engineLock;
			if(m_runner)
				engineLock = m_runner->lockEngine();
			flash = m_engine->userState();
			for(size_t i = 0; i < knobs.size(); ++i)
				knobs[i] = m_engine->mc().adc(static_cast<uint8_t>(i));
		}
		auto xml = stateXml();
		xml.createNewChildElement("Flash")->addTextElement(packBytes(flash));
		xml.createNewChildElement("Knobs")->addTextElement(packBytes(knobs));
		// What each slot holds, which the flash does not (issue #25): as the keeper last read it.
		if(m_keeper)
			xml.createNewChildElement("Slots")->addTextElement(packBytes(g1app::SlotKeeper::pack(m_keeper->slots())));
		juce::MemoryBlock out;
		copyXmlToBinary(xml, out);
		return out;
	}

	bool Processor::applyState(g1app::Engine& _engine, const juce::MemoryBlock& _state)
	{
		const auto xml = getXmlFromBinary(_state.getData(), static_cast<int>(_state.getSize()));
		m_keepProjectState = false;
		if(!xml || !xml->hasTagName(g_stateTag))
		{
			m_origin = "the factory flash: the project's state is not a G1-Emu one";
			return false;
		}
		readPreferences(*xml);
		programsFromString(xml->getStringAttribute("programs"));

		std::vector<uint8_t> flash;
		std::string error = "unreadable";
		const auto* flashXml = xml->getChildByName("Flash");
		if(!flashXml)
		{
			// Saved before the G1 ever ran (settingsOnlyState): it starts as a new instance does,
			// with the knobs where the host had turned them.
			startFromStandalone(_engine);
			m_unstarted = true;
			const auto knobs = juce::StringArray::fromTokens(xml->getStringAttribute("knobs"), " ", {});
			for(int k = 0; k < knobs.size() && k < 18; ++k)
				_engine.mc().setAdc(g1::KnobMap::KnobAdc[static_cast<size_t>(k)], static_cast<uint8_t>(juce::jlimit(0, 255, knobs[k].getIntValue())));
			if(xml->hasAttribute("volume"))
				_engine.mc().setAdc(g1::g_adcVolume, static_cast<uint8_t>(juce::jlimit(0, 255, xml->getIntAttribute("volume"))));
			return true;
		}
		if( !unpackBytes(flashXml->getAllSubText(), flash) || !_engine.setUserState(flash, error))
		{
			// Kept as it came, and handed back as it came: saving the project again must not
			// replace the user's banks with an empty G1 just because this ROM is not that one.
			m_keepProjectState = true;
			m_origin = "the factory flash: the project's G1 state was not used (" + error + "); it is kept in the project untouched";
			return false;
		}
		std::vector<uint8_t> knobs;
		if(const auto* knobsXml = xml->getChildByName("Knobs"); knobsXml && unpackBytes(knobsXml->getAllSubText(), knobs))
			for(size_t i = 0; i < knobs.size() && i < 256; ++i)
				_engine.mc().setAdc(static_cast<uint8_t>(i), knobs[i]);
		// The slots go back once the G1 is up (after the Program Changes, which they override).
		// A project saved before the keeper existed has none: the keeper reads what there is.
		std::vector<uint8_t> slotBytes;
		g1app::SlotKeeper::Slots slots;
		if(const auto* slotsXml = xml->getChildByName("Slots"); m_keeper && slotsXml
			&& unpackBytes(slotsXml->getAllSubText(), slotBytes) && g1app::SlotKeeper::unpack(slotBytes, slots))
			m_keeper->restore(slots);
		m_origin = "this project";
		return true;
	}

	void Processor::getStateInformation(juce::MemoryBlock& _dest)
	{
		std::lock_guard<std::mutex> lock(m_lifecycle);
		if(!m_engine || m_keepProjectState)
		{
			_dest = m_haveState ? m_state : settingsOnlyState();
			return;
		}
		// A G1 that has not run yet has nothing of its own: its banks are the standalone's copy
		// it started from, so it is saved as it was loaded, settings and knobs only.
		if(m_unstarted)
		{
			_dest = settingsOnlyState();
			return;
		}
		_dest = snapshotState();
	}

	void Processor::restart()
	{
		juce::MemoryBlock state;
		getStateInformation(state);
		setStateInformation(state.getData(), static_cast<int>(state.getSize()));
	}

	void Processor::setStateInformation(const void* _data, const int _size)
	{
		juce::MemoryBlock state(_data, static_cast<size_t>(std::max(_size, 0)));
		{
			std::lock_guard<std::mutex> lock(m_pendingMutex);
			m_pending = std::move(state);
		}
		// The engine and the editor's panel are replaced on the message thread only.
		if(juce::MessageManager::getInstanceWithoutCreating() && juce::MessageManager::getInstance()->isThisTheMessageThread())
		{
			cancelPendingUpdate();
			handleAsyncUpdate();
		}
		else
			triggerAsyncUpdate();
	}

	void Processor::handleAsyncUpdate()
	{
		juce::MemoryBlock state;
		{
			std::lock_guard<std::mutex> lock(m_pendingMutex);
			state = std::move(m_pending);
			m_pending.reset();
		}
		if(state.isEmpty())
			return;

		if(auto* editor = dynamic_cast<Editor*>(getActiveEditor()))
			editor->engineGoing();
		suspendProcessing(true);
		{
			std::lock_guard<std::mutex> lock(m_lifecycle);
			m_state = std::move(state);
			m_haveState = true;
			if(m_rom.empty())
			{
				// Still read for the panel's preferences, and kept whole for getStateInformation.
				if(const auto xml = getXmlFromBinary(m_state.getData(), static_cast<int>(m_state.getSize())))
					readPreferences(*xml);
			}
			else
			{
				createEngine(&m_state);
				startRunner();
			}
		}
		suspendProcessing(false);
		// The knobs took the project's positions: the host reads the parameters again, as after
		// loading a preset (CLAP asks for that, or it takes them for its own changes).
		updateHostDisplay(ChangeDetails().withProgramChanged(true));
	}

	// Audio thread: a parameter the host moved turns its knob. A position goes to the G1 only when
	// the parameter really changed, so loading a project or a patch never turns a knob by itself.
	void Processor::knobsFromHost()
	{
		std::unique_lock<std::mutex> lock(m_knobMutex, std::try_to_lock);
		if(!lock || !m_engine || m_knobGeneration != m_generation.load())
			return;
		auto& mc = m_engine->mc();
		for(size_t k = 0; k < 18; ++k)
		{
			const float v = m_knobParams[k]->get();
			if(v == m_lastParam[k])
				continue;
			m_lastParam[k] = v;
			const auto adc = KnobParameter::toAdc(v);
			mc.setAdc(g1::KnobMap::KnobAdc[k], adc);
			m_lastAdc[k] = adc;
		}
		if(const int v = m_volumeParam->get(); v != m_lastVolumeParam)
		{
			m_lastVolumeParam = v;
			m_lastVolumeAdc = VolumeParameter::toAdc(v);
			mc.setAdc(g1::g_adcVolume, static_cast<uint8_t>(m_lastVolumeAdc));
		}
	}

	// Message thread: knobs turned by anything but the host go to the host, and the parameters'
	// names follow what the knobs are assigned to.
	void Processor::timerCallback()
	{
		bool renamed = false;
		{
			std::lock_guard<std::mutex> lifecycle(m_lifecycle);
			if(!m_engine)
				return;
			auto& mc = m_engine->mc();
			knobsToHost(mc);
			g1::KnobMap map(mc);
			for(uint32_t k = 0; k < 18; ++k)
				renamed = m_knobParams[k]->setInfo(map.read(k)) || renamed;
		}
		// Outside the locks: the host asks for the new names right away.
		if(renamed)
			updateHostDisplay(ChangeDetails().withParameterInfoChanged(true));
	}

	// Each knob that something other than the host turned, to its parameter, as a gesture.
	void Processor::knobsToHost(g1::Microcontroller& _mc)
	{
		std::lock_guard<std::mutex> lock(m_knobMutex);
		const auto gesture = [](juce::RangedAudioParameter& _p, const float _value)
		{
			_p.beginChangeGesture();
			_p.setValueNotifyingHost(_value);
			_p.endChangeGesture();
		};
		for(size_t k = 0; k < 18; ++k)
		{
			const int adc = _mc.adc(g1::KnobMap::KnobAdc[k]);
			if(adc == m_lastAdc[k])
				continue;
			m_lastAdc[k] = adc;
			gesture(*m_knobParams[k], KnobParameter::toParam(static_cast<uint8_t>(adc)));
			m_lastParam[k] = m_knobParams[k]->get();
		}
		// The volume's parameter moves by whole steps: a position that keeps the step tells nothing.
		const int adc = _mc.adc(g1::g_adcVolume);
		if(adc == m_lastVolumeAdc)
			return;
		m_lastVolumeAdc = adc;
		if(const int v = VolumeParameter::fromAdc(adc); v != m_volumeParam->get())
			gesture(*m_volumeParam, m_volumeParam->convertTo0to1(static_cast<float>(v)));
		m_lastVolumeParam = m_volumeParam->get();
	}

	// ____________________________________________________________________________________________
	// For the editor

	g1app::Engine* Processor::engine()
	{
		std::lock_guard<std::mutex> lock(m_lifecycle);
		return m_engine.get();
	}

	g1app::HostStats Processor::stats()
	{
		std::lock_guard<std::mutex> lock(m_lifecycle);
		if(m_runner)
			return m_runner->stats();
		g1app::HostStats s;
		s.audio = m_engine ? "waiting for the host to start the audio" : "no ROM";
		s.midi = "the DAW track";
		return s;
	}

	std::string Processor::describe()
	{
		std::lock_guard<std::mutex> lock(m_lifecycle);
		std::string d = "ROM: " + (m_romPath.empty() ? std::string("none") : m_romPath) + "\n";
		if(m_runner)
		{
			char buf[160];
			std::snprintf(buf, sizeof(buf), "Latency: %zu frames (%.1f ms at %.0f Hz), reported to the host\n",
				m_runner->latency(), 1000.0 * static_cast<double>(m_runner->latency()) / m_runner->rate(), m_runner->rate());
			d += buf;
		}
		if(m_engine)
			d += "This instance's patches and synth settings came from " + m_origin + ".\n"
				 "They are saved in the DAW project, never in the standalone's flash, and so is the last\n"
				 "Program Change of each channel, sent again when the project opens (the G1 does not\n"
				 "remember what each slot had).\n";
		d += "Notes and controllers come from the track.\n";
		if(!m_pcPort)
			d += "PC Port: " + m_pcProblem + ".";
		else if(m_pcPort->virtualPorts())
			d += "PC Port: the MIDI port \"" + pcPortClient(m_instance)
				+ " PC Port\". Choose it in Animatek NME as input and output to edit this instance.";
		else
			d += "PC Port: " + m_pcPort->describe() + ".";
		d += "\nDirect link: ";
		if(m_link.listening())
			d += "\"" + m_link.name() + "\" on port " + std::to_string(m_link.port())
				+ ". Animatek NME finds it by itself, with no MIDI port.";
		else
			d += m_linkProblem + ".";
		return d;
	}

	juce::AudioProcessorEditor* Processor::createEditor()
	{
		return new Editor(*this);
	}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new g1plugin::Processor();
}
