#include "SynthSettingsView.h"

#include "Overlay.h"

namespace g1gui
{
	namespace
	{
		// The settings window's look: the colours and sizes are the overlay's (Overlay.h).
		const juce::Colour g_noteText = overlay::NoteText;
		constexpr int g_cardW = 780, g_cardH = 400, g_margin = overlay::Margin, g_colW = 252;	// the last column ends at the margin
		constexpr int g_labelW = 100, g_controlW = 120, g_rowH = 22, g_rowStep = 30;
		constexpr int g_headY = 56, g_firstRowY = 84, g_footerY = 346;
		constexpr int g_lowerHeadY = 242;		// the second row of headings, under the longest column
		constexpr float g_textSize = overlay::TextSize;
		constexpr int g_cardNudge = 0;			// down (or up, negative) from the top of the synth's sections

		// Ids are value + offset, as a ComboBox wants them above 0.
		constexpr int g_channelOff = 16, g_tuneOffset = 128;

		juce::String cents(const int _c) { return (_c > 0 ? "+" : "") + juce::String(_c) + " cents"; }
	}

	juce::Font SettingsLook::getComboBoxFont(juce::ComboBox&) { return juce::FontOptions(g_textSize); }
	juce::Font SettingsLook::getPopupMenuFont() { return juce::FontOptions(g_textSize); }

	SynthSettingsView::SynthSettingsView(g1app::SynthSettingsLink& _link) : m_link(_link)
	{
		setWantsKeyboardFocus(true);
		setLookAndFeel(&m_look);

		auto label = [this](juce::Label& _l, const juce::String& _text)
		{
			_l.setFont(juce::FontOptions(g_textSize));
			_l.setText(_text, juce::dontSendNotification);
			_l.setColour(juce::Label::textColourId, juce::Colours::white);
			_l.setJustificationType(juce::Justification::centredRight);
			addAndMakeVisible(_l);
		};
		auto combo = [this](juce::ComboBox& _c)
		{
			_c.onChange = [this] { apply(); };
			addAndMakeVisible(_c);
		};
		auto choices = [&](juce::ComboBox& _c, const juce::StringArray& _items)
		{
			for(int i = 0; i < _items.size(); ++i)
				_c.addItem(_items[i], i + 1);
			combo(_c);
		};

		label(m_nameLabel, "Name");
		m_name.setFont(juce::FontOptions(g_textSize));
		m_name.setIndents(6, 4);
		m_name.setInputRestrictions(16);
		m_name.onReturnKey = [this] { apply(); };
		m_name.onFocusLost = [this] { apply(); };
		addAndMakeVisible(m_name);

		for(size_t i = 0; i < 4; ++i)
		{
			label(m_slotLabels[i], juce::String("Slot ") + static_cast<char>('A' + i));
			for(int ch = 0; ch < 16; ++ch)
				m_channels[i].addItem(juce::String(ch + 1), ch + 1);
			m_channels[i].addItem("Off", g_channelOff + 1);
			combo(m_channels[i]);
		}
		label(m_keyboardLabel, "Keyboard mode");
		choices(m_keyboard, {"Active slot", "Selected slots"});

		label(m_clockLabel, "Source");
		choices(m_clock, {"Internal", "External"});
		label(m_bpmLabel, "Rate (BPM)");
		for(int bpm = 24; bpm <= 240; ++bpm)
			m_bpm.addItem(juce::String(bpm), bpm + 1);
		combo(m_bpm);
		label(m_syncLabel, "Global sync");
		for(int beats = 1; beats <= 32; ++beats)
			m_sync.addItem(juce::String(beats) + (beats == 1 ? " beat" : " beats"), beats);
		combo(m_sync);
		label(m_tuneLabel, "Master tune");
		for(int c = -100; c <= 100; ++c)
			m_tune.addItem(cents(c), c + g_tuneOffset);
		combo(m_tune);

		label(m_knobLabel, "Knob mode");
		choices(m_knob, {"Immediate", "Hook"});
		label(m_pedalLabel, "Pedal polarity");
		choices(m_pedal, {"Normal", "Inverted"});
		label(m_programLabel, "Program change");
		choices(m_program, {"Off", "Receive", "Send", "Send and receive"});
		label(m_localLabel, "Local");
		choices(m_local, {"On", "Off"});
		label(m_ledsLabel, "Editor LEDs");
		choices(m_leds, {"Active", "Off"});
		label(m_velMinLabel, "Minimum");
		label(m_velMaxLabel, "Maximum");
		for(int v = 0; v <= 127; ++v)
		{
			m_velMin.addItem(juce::String(v), v + 1);
			m_velMax.addItem(juce::String(v), v + 1);
		}
		combo(m_velMin);
		combo(m_velMax);

		m_note.setText("Changes reach the G1 at once. To keep them after a restart, press Shift + Store "
			"on the panel (Save Synth Settings).", juce::dontSendNotification);
		m_note.setFont(juce::FontOptions(g_textSize));
		m_note.setColour(juce::Label::textColourId, g_noteText);
		m_note.setJustificationType(juce::Justification::topLeft);
		addAndMakeVisible(m_note);

		m_close.onClick = [this] { close(); };
		addAndMakeVisible(m_close);

		for(auto* c : getChildren())
			c->setEnabled(c == &m_close);	// until the OS has said what it has
	}

	SynthSettingsView::~SynthSettingsView()
	{
		setLookAndFeel(nullptr);
	}

	void SynthSettingsView::open(juce::Component& _behind, const int _faceHeight, const juce::Rectangle<int> _space)
	{
		m_link.read();
		m_backdrop = overlay::backdrop(_behind);
		m_faceHeight = _faceHeight;
		m_space = _space;
		setBounds(_behind.getLocalBounds());
		setVisible(true);
		toFront(true);
		startTimerHz(5);
		timerCallback();
	}

	void SynthSettingsView::close()
	{
		apply();	// a name still being typed
		stopTimer();
		setVisible(false);
		m_backdrop = {};
		if(onClose)
			onClose();
	}

	// Centred across its space, its top on the space's top.
	juce::Rectangle<int> SynthSettingsView::card() const
	{
		const auto space = m_space.isEmpty() ? getLocalBounds() : m_space;
		return juce::Rectangle<int>(g_cardW, g_cardH).withCentre(space.getCentre()).withY(space.getY() + g_cardNudge);
	}

	void SynthSettingsView::paint(juce::Graphics& _g)
	{
		const auto c = card();
		overlay::paintCard(_g, getLocalBounds(), m_backdrop, m_faceHeight, c, "SYNTH SETTINGS");
		_g.setColour(overlay::Rule);
		_g.drawLine(static_cast<float>(c.getX() + g_margin), static_cast<float>(c.getY() + g_footerY),
			static_cast<float>(c.getRight() - g_margin), static_cast<float>(c.getY() + g_footerY));

		_g.setColour(juce::Colours::white);
		_g.setFont(juce::FontOptions(overlay::HeadingSize, juce::Font::bold));
		// Each heading centred over its column's labels and boxes.
		auto heading = [&](const int _col, const int _y, const juce::String& _text)
		{
			_g.drawText(_text, c.getX() + g_margin + _col * g_colW, c.getY() + _y, g_labelW + 8 + g_controlW, 20, juce::Justification::centred);
		};
		heading(0, g_headY, "MIDI CHANNELS");
		heading(0, g_lowerHeadY, "KEYBOARD");
		heading(1, g_headY, "MIDI CLOCK");
		heading(1, g_lowerHeadY, "TUNING");
		heading(2, g_headY, "CONTROL");
		heading(2, g_lowerHeadY, "VELOCITY SCALE");
		if(!m_haveSettings)
		{
			_g.setColour(g_noteText);
			_g.setFont(juce::FontOptions(12.0f));
			_g.drawText("Asking the G1...", c.getX() + 200, c.getY() + 12, 300, 22, juce::Justification::centredLeft);
		}
	}

	void SynthSettingsView::resized()
	{
		const auto c = card();
		auto row = [&](const int _col, const int _y, juce::Label& _label, juce::Component& _control)
		{
			const int x = c.getX() + g_margin + _col * g_colW;
			_label.setBounds(x, c.getY() + _y, g_labelW, g_rowH);
			_control.setBounds(x + g_labelW + 8, c.getY() + _y, g_controlW, g_rowH);
		};
		auto rowY = [](const int _i) { return g_firstRowY + _i * g_rowStep; };
		m_nameLabel.setBounds(c.getRight() - g_margin - 160 - 8 - 60, c.getY() + 12, 60, g_rowH);
		m_name.setBounds(c.getRight() - g_margin - 160, c.getY() + 12, 160, g_rowH);

		for(size_t i = 0; i < 4; ++i)
			row(0, rowY(static_cast<int>(i)), m_slotLabels[i], m_channels[i]);
		row(0, g_lowerHeadY + 28, m_keyboardLabel, m_keyboard);

		row(1, rowY(0), m_clockLabel, m_clock);
		row(1, rowY(1), m_bpmLabel, m_bpm);
		row(1, rowY(2), m_syncLabel, m_sync);
		row(1, g_lowerHeadY + 28, m_tuneLabel, m_tune);

		row(2, rowY(0), m_knobLabel, m_knob);
		row(2, rowY(1), m_pedalLabel, m_pedal);
		row(2, rowY(2), m_programLabel, m_program);
		row(2, rowY(3), m_localLabel, m_local);
		row(2, rowY(4), m_ledsLabel, m_leds);
		row(2, g_lowerHeadY + 28, m_velMinLabel, m_velMin);
		row(2, g_lowerHeadY + 28 + g_rowStep, m_velMaxLabel, m_velMax);

		m_note.setBounds(c.getX() + g_margin, c.getY() + g_footerY + 10, g_cardW - 2 * g_margin - 110, 36);
		m_close.setBounds(c.getRight() - g_margin - 90, c.getY() + g_footerY + 13, 90, 26);
	}

	// A click beside the card closes it, as Escape does.
	void SynthSettingsView::mouseDown(const juce::MouseEvent& _e)
	{
		if(!card().contains(_e.getPosition()))
			close();
	}

	bool SynthSettingsView::keyPressed(const juce::KeyPress& _key)
	{
		if(_key == juce::KeyPress::escapeKey)
		{
			close();
			return true;
		}
		return false;
	}

	void SynthSettingsView::timerCallback()
	{
		g1app::SynthSettings s;
		uint64_t revision = 0;
		if(!m_link.settings(s, revision) || revision == m_revision)
			return;
		m_revision = revision;
		m_known = s;
		if(!m_haveSettings)
		{
			m_haveSettings = true;
			for(auto* c : getChildren())
				c->setEnabled(true);
			repaint();
		}
		show(s);
	}

	void SynthSettingsView::show(const g1app::SynthSettings& _s)
	{
		const auto n = juce::dontSendNotification;
		// A value the lists leave out (the OS's own range is wider) is added, so it shows as it is.
		auto select = [n](juce::ComboBox& _c, const int _id, const juce::String& _text)
		{
			if(_c.indexOfItemId(_id) < 0)
				_c.addItem(_text, _id);
			_c.setSelectedId(_id, n);
		};
		if(!m_name.hasKeyboardFocus(false))
			m_name.setText(_s.name, false);
		for(size_t i = 0; i < 4; ++i)
			select(m_channels[i], std::min(_s.midiChannel[i], g_channelOff) + 1, juce::String(_s.midiChannel[i] + 1));
		m_keyboard.setSelectedId(_s.keyboardMode + 1, n);
		m_clock.setSelectedId(_s.clockInternal ? 1 : 2, n);
		select(m_bpm, _s.clockBpm + 1, juce::String(_s.clockBpm));
		m_sync.setSelectedId(_s.globalSync + 1, n);
		select(m_tune, _s.masterTune + g_tuneOffset, cents(_s.masterTune));
		m_knob.setSelectedId(_s.knobMode + 1, n);
		m_pedal.setSelectedId(_s.pedalPolarity + 1, n);
		m_program.setSelectedId(1 + (_s.programChangeReceive ? 1 : 0) + (_s.programChangeSend ? 2 : 0), n);
		m_local.setSelectedId(_s.localOn ? 1 : 2, n);
		m_leds.setSelectedId(_s.ledsActive ? 1 : 2, n);
		m_velMin.setSelectedId(_s.velScaleMin + 1, n);
		m_velMax.setSelectedId(_s.velScaleMax + 1, n);
	}

	void SynthSettingsView::apply()
	{
		if(!m_haveSettings)
			return;
		auto s = m_known;
		s.name = m_name.getText().substring(0, 16).toStdString();
		for(size_t i = 0; i < 4; ++i)
			s.midiChannel[i] = m_channels[i].getSelectedId() - 1;
		s.keyboardMode = m_keyboard.getSelectedId() - 1;
		s.clockInternal = m_clock.getSelectedId() == 1;
		s.clockBpm = m_bpm.getSelectedId() - 1;
		s.globalSync = m_sync.getSelectedId() - 1;
		s.masterTune = m_tune.getSelectedId() - g_tuneOffset;
		s.knobMode = m_knob.getSelectedId() - 1;
		s.pedalPolarity = m_pedal.getSelectedId() - 1;
		const int program = m_program.getSelectedId() - 1;
		s.programChangeReceive = (program & 1) != 0;
		s.programChangeSend = (program & 2) != 0;
		s.localOn = m_local.getSelectedId() == 1;
		s.ledsActive = m_leds.getSelectedId() == 1;
		s.velScaleMin = m_velMin.getSelectedId() - 1;
		s.velScaleMax = m_velMax.getSelectedId() - 1;
		if(s == m_known)
			return;		// nothing changed (the name editor losing the focus)
		m_known = s;
		m_link.write(s);
	}
}
