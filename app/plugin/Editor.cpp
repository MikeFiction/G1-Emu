#include "Editor.h"

#include "Processor.h"

#include "romfinder.h"

namespace g1plugin
{
	Editor::Editor(Processor& _processor) : juce::AudioProcessorEditor(_processor), m_processor(_processor)
	{
		m_message.setColour(juce::Label::textColourId, juce::Colours::white);
		m_message.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
		m_message.setJustificationType(juce::Justification::topLeft);
		addChildComponent(m_message);

		m_openFolder.onClick = []
		{
			const juce::File folder(g1app::publicRomFolder());
			(void)folder.createDirectory();
			folder.revealToUser();
		};
		m_pickRom.onClick = [this] { chooseRom(); };
		addChildComponent(m_openFolder);
		addChildComponent(m_pickRom);

		setSize(1200, 440);
		rebuild();
		startTimerHz(5);
	}

	Editor::~Editor()
	{
		stopTimer();
		m_panel.reset();
	}

	void Editor::engineGoing()
	{
		m_panel.reset();
		m_engine = nullptr;
		m_generation = -1;
	}

	void Editor::timerCallback()
	{
		if(m_generation != m_processor.generation())
			rebuild();
	}

	void Editor::rebuild()
	{
		m_panel.reset();
		m_generation = m_processor.generation();
		m_engine = m_processor.engine();

		const bool noRom = !m_processor.romProblem().empty();
		m_message.setVisible(!m_engine);
		m_openFolder.setVisible(!m_engine && noRom);
		m_pickRom.setVisible(!m_engine && noRom);
		if(!m_engine)
		{
			m_message.setText(noRom ? juce::String(m_processor.romProblem()) : juce::String("Waiting for the host to start the audio..."),
				juce::dontSendNotification);
			// Until the host starts the audio there is nothing to show, and nothing to poll for
			// but the generation: the timer keeps running.
			setSize(1200, 440);
			resized();
			return;
		}
		m_panel = std::make_unique<g1gui::Panel>(static_cast<g1gui::PanelHost&>(*this));
		addAndMakeVisible(*m_panel);
		setSize(m_panel->getWidth(), m_panel->getHeight());
	}

	void Editor::paint(juce::Graphics& _g)
	{
		_g.fillAll(juce::Colour(0xff2b2346));
	}

	void Editor::resized()
	{
		if(m_panel)
			m_panel->setTopLeftPosition(0, 0);
		auto area = getLocalBounds().reduced(24);
		auto buttons = area.removeFromBottom(32);
		m_pickRom.setBounds(buttons.removeFromRight(200));
		buttons.removeFromRight(12);
		m_openFolder.setBounds(buttons.removeFromRight(200));
		m_message.setBounds(area);
	}

	// The panel changes its own height when the extras drawer opens: the editor follows.
	void Editor::childBoundsChanged(juce::Component* _child)
	{
		if(_child == m_panel.get())
			setSize(m_panel->getWidth(), m_panel->getHeight());
	}

	void Editor::chooseRom()
	{
		m_chooser = std::make_unique<juce::FileChooser>("Choose the Nord Modular rack ROM",
			juce::File(g1app::publicRomFolder()), "*.bin;*.BIN");
		m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
			[this](const juce::FileChooser& _c)
		{
			const auto file = _c.getResult();
			if(file != juce::File())
				m_processor.useRom(file);	// the generation changes: the timer rebuilds
		});
	}

	g1app::HostStats Editor::stats() { return m_processor.stats(); }
	bool Editor::extrasOpen() const { return m_processor.extrasOpen(); }
	void Editor::setExtrasOpen(const bool _open) { m_processor.setExtrasOpen(_open); }
	bool Editor::knobDisplays() const { return m_processor.knobDisplays(); }
	void Editor::setKnobDisplays(const bool _on) { m_processor.setKnobDisplays(_on); }

	void Editor::showSettings(juce::Component* _parent)
	{
		juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "G1-Emu",
			juce::String(m_processor.describe()) + "\n\n" + g1gui::disclaimerText(), "OK", _parent);
	}
}
