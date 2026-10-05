// g1gui: the emulated G1 with its panel in a window.
//
//   g1gui [ROM] [FLASH]
//
// Without a ROM argument it looks for one: the settings file first, then the ROM folders
// (romfinder.h). With none anywhere it says what to put where and offers to open the folder or to
// pick a file, and starts as soon as it has one. The flash works like g1run (default
// the per-user G1-Emu data directory). Everything else (MIDI, JACK audio, G1_* variables) is
// the same as in g1run: EmuHost does it. What the window lets the user choose is in the settings
// window (Settings.h), the notice included.

#include "Panel.h"

#include "Settings.h"

#include "romfinder.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace g1gui
{
	// The panel's view of EmuHost: its preferences go to the settings file, and Settings opens
	// the settings window.
	class WindowHost : public PanelHost
	{
	public:
		explicit WindowHost(g1app::EmuHost& _host) : m_host(_host) {}
		g1::Microcontroller& mc() override { return m_host.mc(); }
		g1app::HostStats stats() override { return m_host.stats(); }
		bool extrasOpen() const override { return m_host.options().extrasOpen; }
		void setExtrasOpen(const bool _open) override { m_host.options().extrasOpen = _open; save(); }
		bool knobDisplays() const override { return m_host.options().knobDisplays; }
		void setKnobDisplays(const bool _on) override { m_host.options().knobDisplays = _on; save(); }
		bool knobFollowsPatch() const override { return m_host.options().knobFollowsPatch; }
		void setKnobFollowsPatch(const bool _on) override { m_host.options().knobFollowsPatch = _on; save(); }
		float panelScale() const override { return m_host.options().panelScale; }
		void setPanelScale(const float _scale) override { m_host.options().panelScale = _scale; }	// saved when the window closes
		juce::String settingsTooltip() const override { return "Audio driver, output level and raw MIDI"; }
		void showSettings(juce::Component* _parent) override { SettingsView::show(m_host, _parent); }
		g1app::SynthSettingsLink& synthSettings() override { return m_host.synthSettings(); }
		void save() { m_host.options().save(g1app::EmuHost::defaultSettingsPath()); }
		// The master volume as it was left: put back before the panel reads it, kept when it closes.
		void restoreVolume()
		{
			if(const int v = m_host.options().masterVolume; v >= 0 && v <= 255)
				m_host.mc().setAdc(Panel::VolumeAdc, static_cast<uint8_t>(v));
		}
		void keepVolume() { m_host.options().masterVolume = m_host.mc().adc(Panel::VolumeAdc); }
	private:
		g1app::EmuHost& m_host;
	};

	// JUCE hands a desktop window's constrainer its bounds with the native frame around them; the
	// panel's proportions and the size limits are the client area's.
	class ClientConstrainer : public juce::ComponentBoundsConstrainer
	{
	public:
		explicit ClientConstrainer(juce::Component& _window) : m_window(_window) {}
		void checkBounds(juce::Rectangle<int>& _bounds, const juce::Rectangle<int>& _previous, const juce::Rectangle<int>& _limits,
			const bool _top, const bool _left, const bool _bottom, const bool _right) override
		{
			juce::BorderSize<int> frame;
			if(auto* peer = m_window.getPeer())
				if(const auto size = peer->getFrameSizeIfPresent())
					frame = *size;
			auto client = frame.subtractedFrom(_bounds);
			ComponentBoundsConstrainer::checkBounds(client, frame.subtractedFrom(_previous), frame.subtractedFrom(_limits), _top, _left, _bottom, _right);
			_bounds = frame.addedTo(client);
		}
	private:
		juce::Component& m_window;
	};

	class MainWindow : public juce::DocumentWindow
	{
	public:
		MainWindow(g1app::EmuHost& _host) : DocumentWindow("G1-Emu", juce::Colours::black, DocumentWindow::closeButton | DocumentWindow::minimiseButton), m_panelHost(_host)
		{
			setUsingNativeTitleBar(true);
			m_panelHost.restoreVolume();
			// Any size, in the panel's proportions, which change with the extras drawer.
			auto* view = new PanelView(m_panelHost);
			setContentOwned(view, true);
			setResizable(true, false);
			// The corner's grip, as the plugin has it: JUCE hides its own on a native title bar.
			m_grip = std::make_unique<juce::ResizableCornerComponent>(this, &m_constrainer);
			m_grip->setAlwaysOnTop(true);
			juce::Component::addAndMakeVisible(*m_grip);
			view->applyLimits(m_constrainer);
			view->onAspectChanged = [this, view] { view->applyLimits(m_constrainer); };
			setConstrainer(&m_constrainer);
			centreWithSize(getWidth(), getHeight());
			m_titleBar = std::make_unique<NativeTitleBarTheme>(*this);	// before it shows, or it flashes light
			setVisible(true);
		}
		~MainWindow() override
		{
			setConstrainer(nullptr);
			m_panelHost.keepVolume();
			m_panelHost.save();	// the size and the volume it was left at
		}
		void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
		void resized() override
		{
			DocumentWindow::resized();
			if(m_grip)
				m_grip->setBounds(getWidth() - 18, getHeight() - 18, 18, 18);
		}
	private:
		WindowHost m_panelHost;
		ClientConstrainer m_constrainer{*this};
		std::unique_ptr<NativeTitleBarTheme> m_titleBar;	// light or dark, as Windows is
		std::unique_ptr<juce::ResizableCornerComponent> m_grip;
	};

	class App : public juce::JUCEApplication
	{
	public:
		const juce::String getApplicationName() override { return "G1-Emu"; }
		const juce::String getApplicationVersion() override { return "0.1"; }
		bool moreThanOneInstanceAllowed() override { return false; }

		void initialise(const juce::String& _cmd) override
		{
			const auto args = juce::StringArray::fromTokens(_cmd, true);
			m_rom = args.isEmpty() ? juce::String() : args[0];
			m_flash = args.size() > 1 ? args[1] : juce::String();
			// No settings file means this is the first run on this machine: the notice is shown
			// once, and answering it writes the file, so it does not come back unless it is
			// turned on again in the settings window.
			m_firstRun = !m_host.options().load(g1app::EmuHost::defaultSettingsPath());
			tryStart();
		}

		void shutdown() override
		{
			m_window.reset();
			m_host.stop();	// saves the flash
		}

	private:
		// Starts the G1, or explains why it cannot. Not finding a ROM is the normal first run of
		// a fresh install, not an error: the user is offered the folder to put one in and a file
		// picker, and this is called again as soon as there is something to try.
		void tryStart()
		{
			std::string log;
			if(!m_host.start(m_rom.toStdString(), m_flash.toStdString(), log))
			{
				if(m_host.romProblem().empty())
					juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "G1-Emu",
						"Cannot start the emulated G1.\n\n" + juce::String(log));
				else
					askForRom();
				return;
			}
			std::printf("%s", log.c_str());
			std::fflush(stdout);	// the window has no console reading it as it goes
			m_window = std::make_unique<MainWindow>(m_host);
			if(m_firstRun || m_host.options().showDisclaimer)
				showDisclaimer(m_firstRun);
		}

		// The ROM is the user's to provide and always will be, so this is the one screen a new
		// user is sure to meet. It has to say what is needed, where it goes, and take them there.
		void askForRom()
		{
			auto* w = new juce::AlertWindow("G1-Emu needs a ROM", m_host.romProblem(), juce::MessageBoxIconType::WarningIcon);
			w->addButton("Open the folder", 1);
			w->addButton("Choose a ROM file...", 2);
			w->addButton("Quit", 0, juce::KeyPress(juce::KeyPress::escapeKey));
			w->enterModalState(true, juce::ModalCallbackFunction::create([this](const int _result)
			{
				if(_result == 1)
				{
					const juce::File folder(g1app::publicRomFolder());
					(void)folder.createDirectory();
					folder.revealToUser();
					askForRom();		// it is still not started: ask again once they are back
				}
				else if(_result == 2)
					chooseRom();
				else
					juce::JUCEApplication::getInstance()->systemRequestedQuit();
			}), true);
		}

		void chooseRom()
		{
			m_chooser = std::make_unique<juce::FileChooser>("Choose the Nord Modular rack ROM",
				juce::File(g1app::publicRomFolder()), "*.bin;*.BIN");
			m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
				[this](const juce::FileChooser& _c)
			{
				const auto file = _c.getResult();
				if(file == juce::File())
				{
					askForRom();
					return;
				}
				// Remembered so the next run starts straight away, wherever the file lives.
				m_host.options().rom = file.getFullPathName().toStdString();
				m_host.options().save(g1app::EmuHost::defaultSettingsPath());
				m_rom = {};		// the settings carry it now; a bad one must not stop the search
				tryStart();
			});
		}

		// The notice, shown on the first run and afterwards only if the settings window asks
		// for it. It is always readable there, so there is no "don't show this again" to tick:
		// closing it is enough, and the answer is written to the settings file.
		void showDisclaimer(const bool _firstRun)
		{
			auto* w = new juce::AlertWindow("Please read this first",
				juce::String(disclaimerText()) + "\n\nYou can read this again in Settings, which is also where "
				"to turn it back on at startup.", juce::MessageBoxIconType::InfoIcon, m_window.get());
			w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey), juce::KeyPress(juce::KeyPress::escapeKey));
			w->enterModalState(true, juce::ModalCallbackFunction::create([this, _firstRun](int)
			{
				if(!_firstRun)
					return;
				m_host.options().showDisclaimer = false;
				m_host.options().save(g1app::EmuHost::defaultSettingsPath());
			}), true);
		}

		g1app::EmuHost m_host;
		std::unique_ptr<MainWindow> m_window;
		std::unique_ptr<juce::FileChooser> m_chooser;
		juce::String m_rom, m_flash;
		bool m_firstRun = false;
	};
}

START_JUCE_APPLICATION(g1gui::App)
