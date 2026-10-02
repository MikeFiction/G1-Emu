// g1vst3check: loads the built G1-Emu.vst3 through JUCE's VST3 host, as a DAW would, and plays it.
//
//   g1vst3check path/to/G1-Emu.vst3
//
// Two instances at once in one process, each with its own Program Change and a note, rendered
// offline block by block and interleaved; then the first one's state goes into a third instance,
// which must come back with the same reported latency and play its patch without being told; and the editor is opened and
// closed when there is a display. While A and B play, each one's PC Port gets the editor's greeting
// (IAm) through its virtual MIDI port, as Animatek NME sends it, and must answer on its own port and
// not on the other's (Linux and macOS: on Windows JUCE makes no virtual ports). Needs a ROM where the plugin looks for one: without it, 77.

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{
	constexpr double g_rate = 48000.0;
	constexpr int g_block = 512;

	struct Voice
	{
		std::unique_ptr<juce::AudioPluginInstance> plugin;
		int program = -1;
		float peakBefore = 0, peakAfter = 0;
	};

	std::unique_ptr<juce::AudioPluginInstance> load(juce::AudioPluginFormatManager& _formats, const juce::String& _path)
	{
		juce::OwnedArray<juce::PluginDescription> found;
		juce::VST3PluginFormat vst3;
		vst3.findAllTypesForFile(found, _path);
		if(found.isEmpty())
			return {};
		juce::String error;
		auto p = _formats.createPluginInstance(*found[0], g_rate, g_block, error);
		if(!p)
			std::printf("cannot instantiate: %s\n", error.toRawUTF8());
		return p;
	}

	// An editor at the other end of the instances' PC Ports: what each port has answered.
	class Editor final : public juce::MidiInputCallback
	{
	public:
		bool open(const juce::String& _port)
		{
			std::unique_ptr<juce::MidiOutput> out;
			std::unique_ptr<juce::MidiInput> in;
			for(const auto& d : juce::MidiOutput::getAvailableDevices())
				if(d.name == _port)
					out = juce::MidiOutput::openDevice(d.identifier);
			for(const auto& d : juce::MidiInput::getAvailableDevices())
				if(d.name == _port)
					in = juce::MidiInput::openDevice(d.identifier, this);
			if(!out || !in)
				return false;
			in->start();
			m_names[in.get()] = _port;
			m_outs[_port] = std::move(out);
			m_ins.push_back(std::move(in));
			return true;
		}
		void greet(const juce::String& _port)
		{
			const uint8_t iAm[] = {0xf0, 0x33, 0x00, 0x06, 0x00, 0x03, 0x03, 0xf7};
			m_outs[_port]->sendMessageNow(juce::MidiMessage(iAm, sizeof(iAm)));
		}
		void handleIncomingMidiMessage(juce::MidiInput* _source, const juce::MidiMessage& _m) override
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			// Only the answer to IAm (F0 33 00 06 01 03 ...): the G1 also speaks unasked on this
			// port, at boot and at every Program Change.
			const auto* d = _m.getRawData();
			if(_m.getRawDataSize() > 6 && d[0] == 0xf0 && d[1] == 0x33 && d[2] == 0x00 && d[4] == 0x01 && d[5] == 0x03)
				++m_replies[m_names[_source]];
		}
		int replies(const juce::String& _port)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_replies[_port];
		}
	private:
		std::mutex m_mutex;
		std::map<juce::MidiInput*, juce::String> m_names;
		std::map<juce::String, int> m_replies;
		std::map<juce::String, std::unique_ptr<juce::MidiOutput>> m_outs;
		std::vector<std::unique_ptr<juce::MidiInput>> m_ins;
	};

	// Plays seconds of audio on every voice, block by block and in turn. A Program Change at
	// 2 s (if the voice has one), the note at 3 s, released at 4.5 s. _onBlock, if given, is
	// called before each round of blocks with its first frame.
	void play(std::vector<Voice*> _voices, const double _seconds, const std::function<void(int)>& _onBlock = {})
	{
		const auto total = static_cast<int>(_seconds * g_rate);
		const int pcAt = static_cast<int>(2.0 * g_rate), onAt = static_cast<int>(3.0 * g_rate), offAt = static_cast<int>(4.5 * g_rate);
		for(auto* v : _voices)
		{
			v->plugin->setNonRealtime(true);
			v->plugin->prepareToPlay(g_rate, g_block);
		}
		for(int pos = 0; pos < total; pos += g_block)
		{
			if(_onBlock)
				_onBlock(pos);
			for(auto* v : _voices)
			{
				auto& p = *v->plugin;
				juce::AudioBuffer<float> buffer(std::max(p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), g_block);
				buffer.clear();
				juce::MidiBuffer midi;
				auto at = [&](const int _frame, const juce::MidiMessage& _m)
				{
					if(_frame >= pos && _frame < pos + g_block)
						midi.addEvent(_m, _frame - pos);
				};
				if(v->program >= 0)
					at(pcAt, juce::MidiMessage::programChange(1, v->program));
				at(onAt, juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)));
				at(offAt, juce::MidiMessage::noteOff(1, 60));
				p.processBlock(buffer, midi);
				float peak = 0;
				for(int c = 0; c < p.getTotalNumOutputChannels(); ++c)
					peak = std::max(peak, buffer.getMagnitude(c, 0, g_block));
				(pos < onAt ? v->peakBefore : v->peakAfter) = std::max(pos < onAt ? v->peakBefore : v->peakAfter, peak);
			}
		}
		for(auto* v : _voices)
			v->plugin->releaseResources();
	}

	float db(const float _v) { return _v > 0 ? 20.0f * std::log10(_v) : -200.0f; }
}

int main(int _argc, char** _argv)
{
	if(_argc < 2)
	{
		std::printf("usage: g1vst3check path/to/G1-Emu.vst3\n");
		return 2;
	}
	juce::ScopedJuceInitialiser_GUI juce;
	juce::AudioPluginFormatManager formats;
	formats.addFormat(new juce::VST3PluginFormat());
	const juce::String path(_argv[1]);
	int failed = 0;

	Voice a, b;
	a.plugin = load(formats, path);
	b.plugin = load(formats, path);
	if(!a.plugin || !b.plugin)
		return 1;
	auto& p = *a.plugin;
	std::printf("%s by %s: %d in, %d out (%d buses), accepts MIDI %d, synth %d\n", p.getName().toRawUTF8(),
		p.getPluginDescription().manufacturerName.toRawUTF8(), p.getTotalNumInputChannels(), p.getTotalNumOutputChannels(),
		p.getBusCount(false), p.acceptsMidi(), p.getPluginDescription().isInstrument);

	// The editor on both PC Ports. The greeting goes once a second from 1 s on: the OS may still
	// be booting at the first one. Rendering is offline, so the replies get time to come back by
	// sleeping a little at each greeting.
	Editor editor;
#if defined(_WIN32)
	const bool havePorts = false;
#else
	const bool havePorts = editor.open("G1-Emu PC Port") && editor.open("G1-Emu 2 PC Port");
	if(!havePorts)
	{
		std::printf("FAIL: the instances' PC Ports (\"G1-Emu PC Port\", \"G1-Emu 2 PC Port\") are not there\n");
		failed = 1;
	}
#endif
	auto greet = [&](const int _pos)
	{
		if(!havePorts || _pos < static_cast<int>(g_rate) || _pos % static_cast<int>(g_rate) >= g_block)
			return;
		editor.greet("G1-Emu PC Port");
		juce::Thread::sleep(100);
	};

	a.program = 0;
	b.program = 1;
	play({&a, &b}, 5.5, greet);
	if(havePorts)
	{
		juce::Thread::sleep(200);
		const int ra = editor.replies("G1-Emu PC Port"), rb = editor.replies("G1-Emu 2 PC Port");
		std::printf("PC Port: A answered IAm %d times, B %d (only A was greeted)\n", ra, rb);
		if(ra == 0 || rb != 0)
		{
			std::printf("FAIL: the PC Port does not answer, or answers on the wrong instance\n");
			failed = 1;
		}
	}
	const auto latency = p.getLatencySamples();
	std::printf("two instances at once: A (program 1) %.1f dBFS before the note, %.1f after; B (program 2) %.1f / %.1f; latency %d frames\n",
		db(a.peakBefore), db(a.peakAfter), db(b.peakBefore), db(b.peakAfter), latency);
	if(a.peakAfter < 1e-3f || b.peakAfter < 1e-3f)
	{
		std::printf("FAIL: an instance is silent after the note (is there a ROM, and patches in the standalone's banks?)\n");
		failed = 1;
	}

	juce::MemoryBlock state;
	p.getStateInformation(state);
	std::printf("state of A: %zu bytes\n", state.getSize());

	// The state into a new instance, before it starts, as a DAW opening a project does. No
	// Program Change this time: the plugin must send the one it remembered from A by itself.
	Voice c;
	c.plugin = load(formats, path);
	c.plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
	play({&c}, 5.5);
	std::printf("restored into C: latency %d frames, %.1f dBFS after the note (no Program Change sent)\n",
		c.plugin->getLatencySamples(), db(c.peakAfter));
	juce::MemoryBlock again;
	c.plugin->getStateInformation(again);
	std::printf("state of C: %zu bytes\n", again.getSize());
	if(c.plugin->getLatencySamples() != latency || again.isEmpty() || c.peakAfter < 1e-3f)
	{
		std::printf("FAIL: the restored instance does not match or does not play\n");
		failed = 1;
	}

	// The 18 knobs as parameters: the host turns knob 1, the G1 keeps it there (nothing turns it
	// back), and a project saved after that brings it back in a new instance.
	{
		auto& cp = *c.plugin;
		auto params = cp.getParameters();
		int knobs = 0;
		for(auto* prm : params)
			if(prm->getName(64).startsWith("Knob "))
				++knobs;
		juce::MessageManager::getInstance()->runDispatchLoopUntil(300);	// the names follow the patch
		std::printf("knob parameters: %d; knob 1 is \"%s\"\n", knobs, params.isEmpty() ? "" : params[0]->getName(64).toRawUTF8());
		params[0]->setValueNotifyingHost(0.75f);
		play({&c}, 1.0);
		juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
		const float kept = params[0]->getValue();
		juce::MemoryBlock withKnob;
		cp.getStateInformation(withKnob);
		Voice d;
		d.plugin = load(formats, path);
		d.plugin->setStateInformation(withKnob.getData(), static_cast<int>(withKnob.getSize()));
		play({&d}, 1.0);
		juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
		const float restored = d.plugin->getParameters()[0]->getValue();
		std::printf("knob 1 set to 0.75 by the host: %.3f after playing (%s), %.3f in a project reopened\n",
			kept, params[0]->getCurrentValueAsText().toRawUTF8(), restored);
		if(knobs != 18 || std::abs(kept - 0.75f) > 0.005f || std::abs(restored - 0.75f) > 0.005f)
		{
			std::printf("FAIL: the knobs as parameters\n");
			failed = 1;
		}
	}

	// A Program Change through the host's program parameter, which is how a VST3 host sends one
	// (VST3 has no Program Change as MIDI): it must reach the G1 on channel 1, and the project
	// remembers it like one from the track.
	{
		Voice e;
		e.plugin = load(formats, path);
		e.plugin->setCurrentProgram(4);
		play({&e}, 1.0);
		juce::MemoryBlock st;
		e.plugin->getStateInformation(st);
		// JUCE's VST3 host wraps the plugin's own state, in base64, in an XML of its own.
		bool remembered = false;
		if(const auto xml = juce::AudioProcessor::getXmlFromBinary(st.getData(), static_cast<int>(st.getSize())))
			if(auto* component = xml->getChildByName("IComponent"))
			{
				juce::MemoryBlock inner;
				inner.fromBase64Encoding(component->getAllSubText());
				const std::string bytes(static_cast<const char*>(inner.getData()), inner.getSize());
				remembered = bytes.find("programs=\"0:-1:-1:4") != std::string::npos;
			}
		std::printf("host program 5: current %d, %s in the project\n", e.plugin->getCurrentProgram(), remembered ? "remembered" : "NOT remembered");
		if(!remembered || e.plugin->getCurrentProgram() != 4)
		{
			std::printf("FAIL: the host's program does not reach the G1\n");
			failed = 1;
		}
	}

	if(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr)
	{
		std::unique_ptr<juce::AudioProcessorEditor> editor(c.plugin->createEditorIfNeeded());
		if(editor)
		{
			editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
			editor->setVisible(true);
			juce::MessageManager::getInstance()->runDispatchLoopUntil(1500);
			std::printf("editor: %d x %d\n", editor->getWidth(), editor->getHeight());
			c.plugin->editorBeingDeleted(editor.get());
			editor.reset();
		}
		else
		{
			std::printf("FAIL: no editor\n");
			failed = 1;
		}
	}

	a.plugin.reset();
	b.plugin.reset();
	c.plugin.reset();
	std::printf("%s\n", failed ? "FAILED" : "all good");
	return failed;
}
