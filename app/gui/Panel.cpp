#include "Panel.h"

#include "Settings.h"
#include "LcdFont.h"

#include <cmath>

namespace g1gui
{
	namespace
	{
		// Knobs 1-18: their ADC multiplexer channel. Checked by assigning a knob to each
		// module and moving each channel: the OS tells the editor which knob moved.
		constexpr std::array<uint8_t, 18> g_knobAdc = {0x31, 0x37, 0x2d, 0x32, 0x28, 0x2e, 0x33, 0x29, 0x2f, 0x34, 0x2a, 0x1a, 0x35, 0x2b, 0x1b, 0x36, 0x2c, 0x1c};
		constexpr uint8_t g_volumeAdc = 0x30;

		// Buttons (matrix row.bit), identified by pressing them one by one with g1patchtest.
		constexpr MatrixBit g_btnA{0, 2}, g_btnB{0, 3}, g_btnC{0, 4}, g_btnD{0, 5};
		constexpr MatrixBit g_btnStore{0, 6}, g_btnSystem{0, 7}, g_btnEdit{1, 2}, g_btnPatchLoad{1, 3};
		constexpr MatrixBit g_btnUp{1, 4}, g_btnLeft{1, 5}, g_btnDown{1, 6}, g_btnRight{1, 7};
		constexpr MatrixBit g_btnPanelSplit{2, 2}, g_btnFind{2, 3}, g_btnAssign{2, 6}, g_btnShift{2, 7};
		// Oct Shift -/+ ({2, 4} and {2, 5}) and their five LEDs belong to the keyboard model: the
		// rack's OS keeps the value per slot but neither lights the LEDs nor transposes anything,
		// so the window leaves them out (#11). See NOTES.md, "The panel".

		// LEDs (active low). Knob LEDs: knob k (0-17) in row k%3, bit 1+k/3.
		constexpr std::array<MatrixBit, 4> g_slotLeds = {MatrixBit{0, 7}, MatrixBit{1, 7}, MatrixBit{2, 7}, MatrixBit{3, 7}};
		constexpr std::array<MatrixBit, 4> g_modeLeds = {MatrixBit{3, 3}, MatrixBit{3, 4}, MatrixBit{3, 5}, MatrixBit{3, 6}};
		constexpr MatrixBit g_panelSplitLed{3, 2};

		const juce::Colour g_chassis(0xffb21f2d), g_face(0xff2b2346), g_panel(0xffc9c9c6), g_groupLine(0xffe0a040);
		const juce::Colour g_textDark(0xff2b2346), g_textLight(0xffe8e8f0);
		constexpr int g_extrasHeight = 60;	// what the extras drawer adds below the panel
	}

	// ________________________________________________________________________
	// Display: 2 x 16 characters of 5x8 dots, like the HD44780.

	void LcdView::paint(juce::Graphics& _g)
	{
		const auto area = getLocalBounds().toFloat();
		_g.setColour(juce::Colour(0xffc4232f));	// the hardware's red frame
		_g.fillRoundedRectangle(area, 6.0f);
		const auto glass = area.reduced(10.0f, 9.0f);
		const bool on = m_lcd.displayOn();
		_g.setColour(on ? juce::Colour(0xffa6c83a) : juce::Colour(0xff7d9434));
		_g.fillRect(glass);
		if(!on)
			return;

		constexpr int cols = 16, rows = 2;
		const float cellW = glass.getWidth() / cols, cellH = glass.getHeight() / rows;
		const float dot = std::min(cellW / 6.0f, cellH / 9.0f);
		const auto cg = m_lcd.cgram();
		const auto& font = lcdFont();
		const auto ink = juce::Colour(0xff1c2a0e), ghost = juce::Colour(0xff9bbb35);

		for(int r = 0; r < rows; ++r)
		{
			const auto text = m_lcd.line(static_cast<uint32_t>(r), cols);
			for(int c = 0; c < cols; ++c)
			{
				const auto ch = c < static_cast<int>(text.size()) ? static_cast<uint8_t>(text[static_cast<size_t>(c)]) : uint8_t(' ');
				const float x0 = glass.getX() + c * cellW + (cellW - dot * 5.0f) * 0.5f;
				const float y0 = glass.getY() + r * cellH + (cellH - dot * 8.0f) * 0.5f;
				for(int y = 0; y < 8; ++y)
					for(int x = 0; x < 5; ++x)
					{
						bool px = false;
						if(ch < 16)
							px = (cg[static_cast<size_t>(ch & 7) * 8 + static_cast<size_t>(y)] & (0x10 >> x)) != 0;
						else if(ch >= 0x20 && ch < 0x80 && y < 7)
							px = (font[static_cast<size_t>(ch - 0x20)][static_cast<size_t>(x)] >> y) & 1;
						_g.setColour(px ? ink : ghost);
						_g.fillRect(x0 + x * dot, y0 + y * dot, dot * 0.86f, dot * 0.86f);
					}
			}
		}
	}

	void LedView::paint(juce::Graphics& _g)
	{
		const auto r = getLocalBounds().toFloat().reduced(1.0f);
		if(m_on)
		{
			_g.setColour(juce::Colour(0x5539ff5a));
			_g.fillEllipse(r.expanded(1.0f));
		}
		_g.setColour(m_on ? juce::Colour(0xff5dff78) : juce::Colour(0xff1d4a26));
		_g.fillEllipse(r);
		_g.setColour(juce::Colours::black.withAlpha(0.7f));
		_g.drawEllipse(r, 1.0f);
	}

	PanelButton::PanelButton(const juce::String& _name, g1::Microcontroller& _mc, const MatrixBit _bit)
		: juce::Button(_name), m_mc(_mc), m_bit(_bit)
	{
		if(!_bit.known())
		{
			setEnabled(false);
			setTooltip(_name + ": not yet identified in the panel matrix");
			return;
		}
		setTooltip(_name);
		onStateChange = [this]
		{
			const bool down = isDown();
			if(down != m_down)
			{
				m_down = down;
				m_mc.setButton(static_cast<uint32_t>(m_bit.row), static_cast<uint32_t>(m_bit.bit), down);
			}
		};
	}

	void PanelButton::paintButton(juce::Graphics& _g, const bool _over, const bool _down)
	{
		auto r = getLocalBounds().toFloat().reduced(1.5f);
		_g.setColour(juce::Colours::black.withAlpha(0.5f));
		_g.fillRoundedRectangle(r.translated(0, 2.0f), 4.0f);
		if(_down)
			r = r.translated(0, 1.5f);
		const auto base = isEnabled() ? juce::Colour(0xff1b1b1e) : juce::Colour(0xff4a4a50);
		_g.setGradientFill(juce::ColourGradient(base.brighter(_over ? 0.35f : 0.2f), r.getX(), r.getY(), base, r.getX(), r.getBottom(), false));
		_g.fillRoundedRectangle(r, 4.0f);
		_g.setColour(juce::Colours::black);
		_g.drawRoundedRectangle(r, 4.0f, 1.0f);
	}

	void IconButton::paintButton(juce::Graphics& _g, const bool _over, const bool _down)
	{
		auto r = getLocalBounds().toFloat().reduced(1.0f);
		if(_down)
			r = r.translated(0, 1.0f);
		_g.setColour(juce::Colour(0xff1b1b1e).brighter(_over ? 0.35f : 0.15f));
		_g.fillRoundedRectangle(r, 4.0f);

		// Drawn on a 24 x 24 grid and scaled into the button.
		juce::Path p;
		switch(m_icon)
		{
		case Icon::Settings:
			// Eight teeth and a ring, all overlapping: non-zero winding fills them as one. The
			// hole is painted over it below, in the button's colour.
			for(int i = 0; i < 8; ++i)
			{
				juce::Path tooth;
				tooth.addRoundedRectangle(10.5f, 1.5f, 3.0f, 5.0f, 0.8f);
				tooth.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::twoPi * static_cast<float>(i) / 8.0f, 12.0f, 12.0f));
				p.addPath(tooth);
			}
			p.addEllipse(4.5f, 4.5f, 15.0f, 15.0f);
			break;
		case Icon::Report:
			p.startNewSubPath(12.0f, 2.0f);
			p.lineTo(23.0f, 21.5f);
			p.lineTo(1.0f, 21.5f);
			p.closeSubPath();
			p = p.createPathWithRoundedCorners(2.0f);
			p.setUsingNonZeroWinding(false);	// the exclamation mark is cut out of the triangle
			p.addRoundedRectangle(10.6f, 8.0f, 2.8f, 7.5f, 1.2f);
			p.addEllipse(10.5f, 16.8f, 3.0f, 3.0f);
			break;
		case Icon::Patreon:
			p.addEllipse(8.0f, 2.0f, 14.0f, 14.0f);
			p.addRectangle(2.0f, 2.0f, 4.0f, 20.0f);
			break;
		case Icon::ExtrasOpen:
		case Icon::ExtrasClose:
		{
			// A chevron: down opens the drawer, up closes it.
			const bool down = m_icon == Icon::ExtrasOpen;
			juce::Path line;
			line.startNewSubPath(3.0f, down ? 8.0f : 16.0f);
			line.lineTo(12.0f, down ? 17.0f : 7.0f);
			line.lineTo(21.0f, down ? 8.0f : 16.0f);
			juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, line);
			p.addRectangle(0.0f, 0.0f, 0.01f, 24.0f);	// keeps the 24 x 24 frame, so it scales like the others
			p.addRectangle(23.99f, 0.0f, 0.01f, 24.0f);
			break;
		}
		}
		const auto icon = r.reduced(5.0f);
		_g.setColour(juce::Colour(0xffe8e8f0).withAlpha(isEnabled() ? 1.0f : 0.4f));
		_g.fillPath(p, p.getTransformToScaleToFit(icon, true));
		if(m_icon == Icon::Settings)
		{
			const float scale = std::min(icon.getWidth(), icon.getHeight()) / 24.0f;
			const float d = 7.0f * scale;
			_g.setColour(juce::Colour(0xff1b1b1e).brighter(_over ? 0.35f : 0.15f));
			_g.fillEllipse(icon.getCentreX() - d / 2.0f, icon.getCentreY() - d / 2.0f, d, d);
		}
	}

	// One detent every 8 pixels of drag, or one per wheel click; the pointer turns with it so
	// that the movement can be seen.
	void DialView::turn(const int _detents)
	{
		if(!_detents)
			return;
		m_mc.turnDial(_detents);
		m_angle += static_cast<float>(_detents) * 0.25f;
		repaint();
	}

	void DialView::mouseDrag(const juce::MouseEvent& _e)
	{
		const int detents = (m_lastY - _e.y) / 8;
		if(detents)
		{
			m_lastY -= detents * 8;
			turn(detents);
		}
	}

	void DialView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& _w)
	{
		turn(_w.deltaY > 0 ? 1 : (_w.deltaY < 0 ? -1 : 0));
	}

	void DialView::paint(juce::Graphics& _g)
	{
		const auto d = getLocalBounds().toFloat().reduced(2.0f);
		_g.setColour(juce::Colours::black.withAlpha(0.4f));
		_g.fillEllipse(d.translated(2.0f, 3.0f));
		_g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3a3e), d.getX(), d.getY(), juce::Colour(0xff0c0c0e), d.getRight(), d.getBottom(), false));
		_g.fillEllipse(d);
		const auto c = d.getCentre();
		const float radius = d.getWidth() * 0.5f;
		_g.setColour(juce::Colours::white.withAlpha(0.75f));
		_g.drawLine({c.getPointOnCircumference(radius * 0.3f, m_angle), c.getPointOnCircumference(radius * 0.85f, m_angle)}, 2.5f);
	}

	void KnobLook::drawRotarySlider(juce::Graphics& _g, const int _x, const int _y, const int _w, const int _h, const float _pos, const float _start, const float _end, juce::Slider&)
	{
		const auto area = juce::Rectangle<float>(static_cast<float>(_x), static_cast<float>(_y), static_cast<float>(_w), static_cast<float>(_h)).reduced(3.0f);
		const auto c = area.getCentre();
		const float radius = std::min(area.getWidth(), area.getHeight()) * 0.5f;
		// Red ring with ticks, like the hardware's
		_g.setColour(juce::Colour(0xffc4232f));
		_g.fillEllipse(area);
		_g.setColour(juce::Colours::white.withAlpha(0.8f));
		for(int i = 0; i <= 10; ++i)
		{
			const float a = _start + (_end - _start) * static_cast<float>(i) / 10.0f;
			const auto p1 = c.getPointOnCircumference(radius * 0.97f, a), p2 = c.getPointOnCircumference(radius * 0.82f, a);
			_g.drawLine({p1, p2}, 1.2f);
		}
		// Black body and the white pointer
		const float knob = radius * 0.72f;
		_g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3a3e), c.x - knob, c.y - knob, juce::Colour(0xff0e0e10), c.x + knob, c.y + knob, false));
		_g.fillEllipse(c.x - knob, c.y - knob, knob * 2.0f, knob * 2.0f);
		const float a = _start + _pos * (_end - _start);
		_g.setColour(juce::Colours::white);
		_g.drawLine({c.getPointOnCircumference(knob * 0.25f, a), c.getPointOnCircumference(knob * 0.95f, a)}, 3.0f);
	}

	// ________________________________________________________________________

	Panel::Panel(g1app::EmuHost& _host) : m_host(_host), m_mc(_host.mc()), m_lcd(_host.mc().getLcd()), m_dial(_host.mc())
	{
		addAndMakeVisible(m_lcd);
		addAndMakeVisible(m_dial);

		auto setupKnob = [this](juce::Slider& _s, const uint8_t _adc, const double _initial, const juce::String& _tip)
		{
			_s.setLookAndFeel(&m_knobLook);
			_s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
			_s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
			_s.setRange(0, 255, 1);
			_s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
			_s.setTooltip(_tip);
			_s.setValue(_initial, juce::dontSendNotification);
			_s.onValueChange = [this, &_s, _adc] { m_mc.setAdc(_adc, static_cast<uint8_t>(_s.getValue())); };
			m_mc.setAdc(_adc, static_cast<uint8_t>(_initial));
			addAndMakeVisible(_s);
		};
		setupKnob(m_volume, g_volumeAdc, 255, "Master Volume");
		for(size_t i = 0; i < m_knobs.size(); ++i)
		{
			setupKnob(m_knobs[i], g_knobAdc[i], 0, "Knob " + juce::String(static_cast<int>(i + 1)));
			m_knobLeds[i] = &addLed({static_cast<int>(i % 3), 1 + static_cast<int>(i / 3)});
		}

		m_midiLed = &addLed({});
		m_panelSplitLed = &addLed(g_panelSplitLed);
		m_panelSplit = &addButton("Panel Split", g_btnPanelSplit);
		m_find = &addButton("Find", g_btnFind);

		const char* modes[] = {"Store", "System", "Edit", "Patch/Load"};
		const MatrixBit modeBits[] = {g_btnStore, g_btnSystem, g_btnEdit, g_btnPatchLoad};
		for(size_t i = 0; i < 4; ++i)
		{
			m_modeButtons[i] = &addButton(modes[i], modeBits[i]);
			m_modeLeds[i] = &addLed(g_modeLeds[i]);
		}
		const char* slots[] = {"A", "B", "C", "D"};
		const MatrixBit slotBits[] = {g_btnA, g_btnB, g_btnC, g_btnD};
		for(size_t i = 0; i < 4; ++i)
		{
			m_slotButtons[i] = &addButton(slots[i], slotBits[i]);
			m_slotLeds[i] = &addLed(g_slotLeds[i]);
		}
		m_assign = &addButton("Assign / Morph", g_btnAssign);
		m_shift = &addButton("Shift", g_btnShift);
		m_nav[0] = &addButton("Up", g_btnUp);
		m_nav[1] = &addButton("Left", g_btnLeft);
		m_nav[2] = &addButton("Right", g_btnRight);
		m_nav[3] = &addButton("Down", g_btnDown);

		m_status.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));
		m_status.setColour(juce::Label::textColourId, juce::Colours::white);
		addAndMakeVisible(m_status);

		// Not on the hardware: the audio driver, the level and the raw MIDI card live here
		// instead of only in the G1_* variables.
		m_settings.setTooltip("Audio driver, output level and raw MIDI");
		m_settings.onClick = [this] { SettingsView::show(m_host, this); };
		addAndMakeVisible(m_settings);

		// Also not on the hardware: a new GitHub issue with what we always have to ask for.
		m_report.setTooltip("Open a new issue on GitHub, with this build and setup filled in");
		m_report.onClick = [this] { reportIssue(); };
		addAndMakeVisible(m_report);

		m_patreon.setTooltip("Support G1-Emu and Animatek NME on Patreon");
		m_patreon.onClick = [] { juce::URL("https://www.patreon.com/c/animatek").launchInDefaultBrowser(); };
		addAndMakeVisible(m_patreon);

		m_extras.onClick = [this]
		{
			setExtrasOpen(!m_extrasOpen);
			m_host.options().extrasOpen = m_extrasOpen;
			m_host.options().save(g1app::EmuHost::defaultSettingsPath());
		};
		addAndMakeVisible(m_extras);

		// Random: each of the 18 knobs to a value of its own, as if turned by hand. What it
		// changes is whatever the patch has assigned to them; knobs with nothing assigned do nothing.
		m_random.setTooltip("Turn the 18 knobs to random positions");
		m_random.onClick = [this] { randomizeKnobs(); };
		addChildComponent(m_random);

		setExtrasOpen(m_host.options().extrasOpen);
		startTimerHz(30);
	}

	Panel::~Panel()
	{
		m_volume.setLookAndFeel(nullptr);
		for(auto& k : m_knobs)
			k.setLookAndFeel(nullptr);
	}

	PanelButton& Panel::addButton(const juce::String& _name, const MatrixBit _bit)
	{
		m_buttons.push_back(std::make_unique<PanelButton>(_name, m_mc, _bit));
		addAndMakeVisible(*m_buttons.back());
		return *m_buttons.back();
	}

	LedView& Panel::addLed(const MatrixBit _bit)
	{
		m_leds.push_back(std::make_unique<LedView>());
		addAndMakeVisible(*m_leds.back());
		if(_bit.known())
			m_ledMap.emplace_back(m_leds.back().get(), _bit);
		return *m_leds.back();
	}

	void Panel::paint(juce::Graphics& _g)
	{
		_g.fillAll(g_chassis);
		const juce::Rectangle<float> face(12.0f, 12.0f, static_cast<float>(getWidth()) - 24.0f, 378.0f);
		_g.setColour(g_face);
		_g.fillRoundedRectangle(face, 16.0f);

		// The knob groups and the display area, grey with the orange border
		const juce::Rectangle<float> groups[] = {{140, 26, 196, 330}, {350, 26, 196, 330}, {560, 26, 100, 330}, {674, 26, 100, 330}, {790, 26, 380, 330}};
		for(const auto& g : groups)
		{
			_g.setColour(g_panel);
			_g.fillRoundedRectangle(g, 8.0f);
			_g.setColour(g_groupLine);
			_g.drawRoundedRectangle(g, 8.0f, 1.6f);
		}

		auto label = [&](const juce::String& _t, const juce::Rectangle<int> _r, const juce::Colour _c, const float _size = 11.0f, const juce::Justification _j = juce::Justification::centred)
		{
			_g.setColour(_c);
			_g.setFont(juce::FontOptions(_size, juce::Font::bold));
			_g.drawText(_t, _r, _j, false);
		};

		label("MASTER", {20, 24, 110, 13}, g_textLight, 12.0f);
		label("VOLUME", {20, 37, 110, 13}, g_textLight, 12.0f);
		label("MIDI", m_midiLed->getBounds().withWidth(40).translated(14, -1), g_textLight, 10.0f, juce::Justification::centredLeft);
		label("Panel Split", m_panelSplitLed->getBounds().withWidth(80).translated(14, -1), g_textLight, 11.0f, juce::Justification::centredLeft);
		label("Find", m_find->getBounds().translated(0, -15).withHeight(13), g_textLight);
		label("Panic", m_find->getBounds().translated(0, m_find->getHeight() + 2).withHeight(12), juce::Colour(0xffe0404a), 10.0f);

		for(size_t i = 0; i < m_knobs.size(); ++i)
			label(juce::String(static_cast<int>(i + 1)), m_knobLeds[i]->getBounds().withSizeKeepingCentre(24, 12).translated(0, 13), g_textDark, 10.0f);

		const char* modes[] = {"Store", "System", "Edit", "Patch/Load"};
		for(size_t i = 0; i < 4; ++i)
			label(modes[i], m_modeLeds[i]->getBounds().withWidth(70).translated(14, -1), g_textDark, 11.0f, juce::Justification::centredLeft);
		label("Save", m_modeButtons[0]->getBounds().translated(0, m_modeButtons[0]->getHeight() + 2).withHeight(12), g_textDark, 10.0f);
		label("Synth Settings", m_modeButtons[0]->getBounds().translated(-10, m_modeButtons[0]->getHeight() + 14).withHeight(12).withWidth(m_modeButtons[0]->getWidth() + 20), g_textDark, 10.0f);
		const char* slots[] = {"A", "B", "C", "D"};
		for(size_t i = 0; i < 4; ++i)
			label(slots[i], m_slotLeds[i]->getBounds().withWidth(40).translated(14, -1), g_textDark, 11.0f, juce::Justification::centredLeft);
		label("Navigator", m_nav[0]->getBounds().translated(-24, -15).withWidth(m_nav[0]->getWidth() + 48).withHeight(13), g_textDark);
		label("Assign/Morph", m_assign->getBounds().translated(-14, -15).withWidth(m_assign->getWidth() + 28).withHeight(13), g_textDark, 10.0f);
		label("Shift", m_shift->getBounds().translated(0, -15).withHeight(13), g_textDark);

		if(m_extrasOpen)
		{
			const juce::Rectangle<float> drawer(12.0f, 436.0f, static_cast<float>(getWidth()) - 24.0f, static_cast<float>(g_extrasHeight) - 12.0f);
			_g.setColour(g_face);
			_g.fillRoundedRectangle(drawer, 10.0f);
			label("EXTRAS", drawer.toNearestInt().withWidth(110), g_textLight, 11.0f);
		}

		label("V I R T U A L      M O D U L A R      S Y N T H E S I Z E R      -      G 1 - E M U", {140, 362, 1030, 16}, g_textLight, 10.0f);
	}

	void Panel::resized()
	{
		// Left column
		m_volume.setBounds(42, 54, 66, 66);
		m_midiLed->setBounds(46, 130, 10, 10);
		m_panelSplitLed->setBounds(34, 158, 10, 10);
		m_panelSplit->setBounds(40, 174, 70, 28);
		m_find->setBounds(40, 234, 70, 28);

		// Knobs: 1-3 and 4-6 in the first group, 7-12 in the second, 13-15 and 16-18 in the others
		const int colX[] = {158, 250, 368, 460, 578, 692};
		const int rowY[] = {52, 158, 264};
		for(size_t i = 0; i < m_knobs.size(); ++i)
		{
			const int col = static_cast<int>(i / 3), row = static_cast<int>(i % 3);
			m_knobs[i].setBounds(colX[col], rowY[row], 66, 66);
			m_knobLeds[i]->setBounds(colX[col] + 66, rowY[row] - 14, 10, 10);
		}

		// Right: display, modes, slots, navigator, Assign/Morph, Shift and the dial
		m_lcd.setBounds(810, 40, 250, 74);
		for(size_t i = 0; i < 4; ++i)
		{
			const int x = 810 + static_cast<int>(i) * 64;
			m_modeLeds[i]->setBounds(x + 4, 146, 10, 10);
			m_modeButtons[i]->setBounds(x, 160, 60, 28);
			m_slotLeds[i]->setBounds(x + 4, 262, 10, 10);
			m_slotButtons[i]->setBounds(x, 276, 60, 28);
		}
		m_nav[0]->setBounds(1105, 48, 26, 34);
		m_nav[1]->setBounds(1070, 84, 34, 26);
		m_nav[2]->setBounds(1132, 84, 34, 26);
		m_nav[3]->setBounds(1105, 112, 26, 34);
		m_assign->setBounds(1078, 178, 40, 28);
		m_shift->setBounds(1128, 178, 40, 28);
		m_dial.setBounds(1094, 232, 72, 72);

		// Settings, Report issue and Patreon: three small icon buttons at the right of the status bar
		m_status.setBounds(14, 396, getWidth() - 28 - 4 * 32, 30);
		m_extras.setBounds(getWidth() - 14 - 26 - 96, 399, 26, 24);
		m_random.setBounds(140, 446, 100, 28);
		m_settings.setBounds(getWidth() - 14 - 26, 399, 26, 24);
		m_report.setBounds(getWidth() - 14 - 26 - 32, 399, 26, 24);
		m_patreon.setBounds(getWidth() - 14 - 26 - 64, 399, 26, 24);
	}

	void Panel::setExtrasOpen(const bool _open)
	{
		m_extrasOpen = _open;
		m_extras.setIcon(_open ? IconButton::Icon::ExtrasClose : IconButton::Icon::ExtrasOpen);
		m_extras.setTooltip(_open ? "Hide the extras" : "Extras: Random and more");
		m_random.setVisible(_open);
		setSize(1200, _open ? 440 + g_extrasHeight : 440);	// the window follows its content
	}

	void Panel::randomizeKnobs()
	{
		for(auto& k : m_knobs)
			k.setValue(m_rng.nextInt(256), juce::sendNotificationSync);
	}

	void Panel::reportIssue()
	{
		const auto s = m_host.stats();
		juce::String body;
		body << "**What happens**\n\n\n**How to reproduce it** (attach the patch if one is involved)\n\n\n"
			 << "---\n"
			 << "- G1-Emu: " << G1_BUILD_VERSION << "\n"
			 << "- System: " << juce::SystemStats::getOperatingSystemName() << ", "
			 << juce::SystemStats::getCpuModel() << " (" << juce::SystemStats::getNumCpus() << " threads)\n"
			 << "- Audio: " << juce::String(s.audio) << "\n"
			 << "- MIDI: " << juce::String(s.midi) << (s.rawMidi.empty() ? juce::String() : ", raw: " + juce::String(s.rawMidi)) << "\n"
			 << "- " << juce::String::formatted("speed %.1f%%, load %.0f%%", s.speed, s.load)
			 << ", dropouts " << juce::String(static_cast<juce::int64>(s.xruns)) << "\n";
		juce::URL("https://github.com/animatek/G1-Emu/issues/new?body=" + juce::URL::addEscapeChars(body, true)).launchInDefaultBrowser();
	}

	void Panel::timerCallback()
	{
		m_lcd.repaint();
		for(auto& [led, bit] : m_ledMap)
			led->setOn(!(m_mc.ledRow(static_cast<uint32_t>(bit.row)) & (1u << bit.bit)));

		const auto s = m_host.stats();
		// MIDI LED: lights briefly with whatever comes in on the MIDI port (notes, CC)
		if(s.midiIn != m_lastMidiIn)
		{
			m_lastMidiIn = s.midiIn;
			m_midiHold = 3;
		}
		m_midiLed->setOn(m_midiHold > 0);
		if(m_midiHold > 0)
			--m_midiHold;

		m_peakHold = std::max(static_cast<double>(s.peak), m_peakHold * 0.9);
		juce::String dsp;
		for(bool on : s.dspOn)
			dsp << (on ? "o" : "-");
		// String::formatted uses wide printf on Windows: never pass UTF-8 pointers to %s.
		juce::String status = juce::String::formatted("speed %5.1f%%   load %3.0f%%", s.speed, s.load);
#ifdef __linux__
		status += juce::String::formatted("   CPU %.1f cores", s.cpuCores);
#endif
		status += "   DSP " + dsp + "   output 1/2 ";
		status += m_peakHold > 1e-6 ? juce::String::formatted("%+.0f dB", 20.0 * std::log10(m_peakHold)) : "silence";
		m_status.setText(status + "   dropouts " + juce::String(static_cast<juce::int64>(s.xruns)) + "   |  " + juce::String(s.audio)
			// The byte counters, not just the port names: when an editor says "no response from
			// synth", the first thing anybody needs to know is whether its bytes ever arrived.
			// PC in stuck at 0 means they did not; in moving and out stuck means we do not answer.
			+ juce::String::formatted("  |  PC Port in/out %llu/%llu   MIDI in/out %llu/%llu",
				static_cast<unsigned long long>(s.pcIn), static_cast<unsigned long long>(s.pcOut),
				static_cast<unsigned long long>(s.midiIn), static_cast<unsigned long long>(s.midiOut))
			+ (s.rawMidi.empty() ? juce::String() : "  |  raw: " + juce::String(s.rawMidi)), juce::dontSendNotification);
	}
}
