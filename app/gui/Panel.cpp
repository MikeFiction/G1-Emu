#include "Panel.h"

#include "LcdFont.h"

#include <algorithm>
#include <cmath>

namespace g1gui
{
	namespace
	{
		constexpr const auto& g_knobAdc = g1::KnobMap::KnobAdc;
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

	const char* disclaimerText()
	{
		return
			"G1-Emu is an independent, open-source emulator of the Nord Modular G1.\n\n"
			"- It is not affiliated with, endorsed by or connected to Clavia DMI in any way. "
			"\"Nord\" and \"Nord Modular\" are trademarks of Clavia DMI.\n\n"
			"- No ROMs or firmware are included, and none will ever be provided. Please do not ask "
			"for them: you will not find them here.\n\n"
			"- There is no support. This is a pre-alpha community project, made in spare time. Bug "
			"reports and contributions are welcome on GitHub; requests for help, ROMs or builds are not.";
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
		setTooltip(_name + " (right click: hold it down)");
		setWantsKeyboardFocus(false);	// the keys are the panel's: see Panel::keyPressed
		onStateChange = [this] { update(); };
	}

	void PanelButton::update()
	{
		const bool down = isDown() || held();
		if(down != m_down)
		{
			m_down = down;
			m_mc.setButton(static_cast<uint32_t>(m_bit.row), static_cast<uint32_t>(m_bit.bit), down);
		}
		repaint();
	}

	// A right click (Ctrl+click on a Mac) latches the button instead of pressing it, so several
	// can be down at once: hold A, click B, and the G1 sees both.
	void PanelButton::mouseDown(const juce::MouseEvent& _e)
	{
		if(!_e.mods.isPopupMenu())
			return juce::Button::mouseDown(_e);
		if(isEnabled())
		{
			m_latched = !m_latched;
			update();
		}
	}

	void PanelButton::mouseDrag(const juce::MouseEvent& _e)
	{
		if(!_e.mods.isPopupMenu())
			juce::Button::mouseDrag(_e);
	}

	void PanelButton::mouseUp(const juce::MouseEvent& _e)
	{
		if(!_e.mods.isPopupMenu())
			juce::Button::mouseUp(_e);
	}

	void PanelButton::paintButton(juce::Graphics& _g, const bool _over, const bool _down)
	{
		auto r = getLocalBounds().toFloat().reduced(1.5f);
		_g.setColour(juce::Colours::black.withAlpha(0.5f));
		_g.fillRoundedRectangle(r.translated(0, 2.0f), 4.0f);
		if(_down || held())
			r = r.translated(0, 1.5f);
		const auto base = isEnabled() ? juce::Colour(0xff1b1b1e) : juce::Colour(0xff4a4a50);
		_g.setGradientFill(juce::ColourGradient(base.brighter(_over ? 0.35f : 0.2f), r.getX(), r.getY(), base, r.getX(), r.getBottom(), false));
		_g.fillRoundedRectangle(r, 4.0f);
		_g.setColour(juce::Colours::black);
		_g.drawRoundedRectangle(r, 4.0f, 1.0f);
		// Held without the mouse: lit, so it shows that the G1 sees it down.
		if(held())
		{
			_g.setColour(g_groupLine.withAlpha(0.35f));
			_g.fillRoundedRectangle(r.reduced(2.0f), 3.0f);
			_g.setColour(g_groupLine);
			_g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 2.0f);
		}
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

	void KnobDisplay::set(const g1::KnobInfo& _info)
	{
		const juce::String top = _info.assigned ? juce::String(_info.moduleName) : juce::String();
		juce::String bottom = _info.assigned ? juce::String(_info.paramName) : juce::String();
		juce::String value = _info.assigned && _info.section != 2 ? juce::String(static_cast<int>(_info.value)) : juce::String();
		const auto t = value.isEmpty() ? top : top.substring(0, 10 - value.length()).paddedRight(' ', 11 - value.length()) + value;
		if(t == m_top && bottom == m_bottom && _info.assigned == m_assigned)
			return;
		m_top = t;
		m_bottom = bottom;
		m_assigned = _info.assigned;
		repaint();
	}

	void KnobDisplay::paint(juce::Graphics& _g)
	{
		const auto r = getLocalBounds().toFloat();
		_g.setColour(juce::Colour(0xff1c2a0e));
		_g.fillRoundedRectangle(r, 3.0f);
		const auto glass = r.reduced(1.5f);
		_g.setColour(m_assigned ? juce::Colour(0xffa6c83a) : juce::Colour(0xff7d9434));
		_g.fillRoundedRectangle(glass, 2.0f);
		if(!m_assigned)
			return;
		_g.setColour(juce::Colour(0xff1c2a0e));
		_g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::bold));
		const auto text = glass.reduced(3.0f, 1.0f).toNearestInt();
		_g.drawText(m_top, text.withHeight(text.getHeight() / 2), juce::Justification::centredLeft, false);
		_g.drawText(m_bottom, text.withTrimmedTop(text.getHeight() / 2), juce::Justification::centredLeft, false);
	}

	Panel::Panel(PanelHost& _host) : m_host(_host), m_mc(_host.mc()), m_lcd(_host.mc().getLcd()), m_dial(_host.mc()), m_knobMap(_host.mc())
	{
		addAndMakeVisible(m_lcd);
		addAndMakeVisible(m_dial);

		// Each knob starts where the G1's ADC says it is: the G1 is already running (and in the
		// plugin, the editor comes and goes), so writing a position here would be turning it.
		auto setupKnob = [this](juce::Slider& _s, const uint8_t _adc, const juce::String& _tip)
		{
			_s.setLookAndFeel(&m_knobLook);
			_s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
			_s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
			_s.setRange(0, 255, 1);
			_s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
			_s.setTooltip(_tip);
			_s.setValue(m_mc.adc(_adc), juce::dontSendNotification);
			_s.onValueChange = [this, &_s, _adc] { m_mc.setAdc(_adc, static_cast<uint8_t>(_s.getValue())); };
			addAndMakeVisible(_s);
		};
		setupKnob(m_volume, g_volumeAdc, "Master Volume");
		for(size_t i = 0; i < m_knobs.size(); ++i)
		{
			setupKnob(m_knobs[i], g_knobAdc[i], "Knob " + juce::String(static_cast<int>(i + 1)));
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

		// Not on the hardware: what the host lets the user choose (in the window, the audio driver,
		// the level and the raw MIDI card instead of only the G1_* variables).
		m_settings.setTooltip(m_host.settingsTooltip());
		m_settings.onClick = [this] { m_host.showSettings(this); };
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
			m_host.setExtrasOpen(m_extrasOpen);
		};
		addAndMakeVisible(m_extras);

		// Random: each of the 18 knobs to a value of its own, as if turned by hand. What it
		// changes is whatever the patch has assigned to them; knobs with nothing assigned do nothing.
		m_random.setTooltip("Turn the 18 knobs to random positions (double click: back to the patch's values)");
		m_random.onSingleClick = [this] { randomizeKnobs(); };
		m_random.onDoubleClick = [this] { restoreKnobs(); };
		addChildComponent(m_random);

		m_displaysToggle.setTooltip("Show above each knob the module and parameter it moves");
		m_displaysToggle.setColour(juce::ToggleButton::textColourId, g_textLight);
		m_displaysToggle.onClick = [this]
		{
			setKnobDisplays(m_displaysToggle.getToggleState());
			m_host.setKnobDisplays(m_displaysToggle.getToggleState());
		};
		addChildComponent(m_displaysToggle);
		for(auto& d : m_knobDisplays)
			addChildComponent(d);
		setKnobDisplays(m_host.knobDisplays());

		setExtrasOpen(m_host.extrasOpen());

		// The computer's keyboard holds what a mouse cannot: Shift, and A-D together. The keys
		// reach the panel when it has the focus, which a click anywhere on it gives it.
		setWantsKeyboardFocus(true);
		addMouseListener(this, true);
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
			m_knobDisplays[i].setBounds(colX[col] - 8, rowY[row] - 25, 72, 23);
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
		m_displaysToggle.setBounds(260, 446, 180, 28);
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
		m_displaysToggle.setVisible(_open);
		setSize(1200, _open ? 440 + g_extrasHeight : 440);	// the window follows its content
	}

	void Panel::setKnobDisplays(const bool _on)
	{
		m_displaysToggle.setToggleState(_on, juce::dontSendNotification);
		for(auto& d : m_knobDisplays)
			d.setVisible(_on);
	}

	void Panel::randomizeKnobs()
	{
		// Keep the patch's values the first time, and again once the assignments have changed.
		bool same = m_haveSnapshot;
		std::array<g1::KnobInfo, 18> now;
		for(uint32_t k = 0; k < 18; ++k)
		{
			now[k] = m_knobMap.read(k);
			const auto& a = now[k];
			const auto& b = m_snapshot[k];
			if(a.assigned != b.assigned || a.slot != b.slot || a.section != b.section || a.module != b.module || a.param != b.param || a.type != b.type)
				same = false;
		}
		if(!same)
		{
			m_snapshot = now;
			m_haveSnapshot = true;
		}
		for(auto& k : m_knobs)
			k.setValue(1 + m_rng.nextInt(254), juce::sendNotificationSync);	// 0 and 255 the OS ignores
	}

	void Panel::restoreKnobs()
	{
		if(!m_haveSnapshot)
			return;
		for(uint32_t k = 0; k < 18; ++k)
		{
			const auto& s = m_snapshot[k];
			const auto now = m_knobMap.read(k);
			// Only where the knob still moves the same parameter, and not the morph groups, whose
			// value the snapshot does not have.
			if(!s.assigned || s.section == 2 || !now.assigned || now.slot != s.slot || now.section != s.section || now.module != s.module || now.param != s.param)
				continue;
			m_knobs[k].setValue(g1::KnobMap::positionFor(s.value, s.max), juce::sendNotificationSync);
		}
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
			 << ", dropouts " << juce::String(static_cast<juce::int64>(s.xruns)) << "\n"
			 << (s.dspProblem.empty() ? juce::String() : "- " + juce::String(s.dspProblem) + "\n");
		juce::URL("https://github.com/animatek/G1-Emu/issues/new?body=" + juce::URL::addEscapeChars(body, true)).launchInDefaultBrowser();
	}

	namespace
	{
		constexpr std::array<int, 4> g_slotKeys = {'A', 'B', 'C', 'D'};
	}

	bool Panel::keyPressed(const juce::KeyPress& _key)
	{
		// A-D are the slot buttons, with or without Shift: taken here so the host does not act on them.
		const int code = juce::CharacterFunctions::toUpperCase(static_cast<juce::juce_wchar>(_key.getKeyCode()));
		return std::find(g_slotKeys.begin(), g_slotKeys.end(), code) != g_slotKeys.end();
	}

	bool Panel::keyStateChanged(bool)
	{
		updateHeldKeys();
		return false;
	}

	void Panel::mouseDown(const juce::MouseEvent&)
	{
		if(!hasKeyboardFocus(true))
			grabKeyboardFocus();
	}

	// Shift and A-D on the computer's keyboard hold the panel's Shift and slot buttons, as many at
	// once as are pressed. Shift is read from the system, not from key events, so it also works
	// while the mouse is over the panel without the focus (a plugin window in a DAW); once down it
	// stays down until released wherever the mouse goes. Called on every key change and by the timer,
	// which catches what happens while the panel does not get the events.
	void Panel::updateHeldKeys()
	{
		const bool focused = hasKeyboardFocus(true);
		const bool shift = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
		m_shiftKey = shift && (m_shiftKey || focused || isMouseOver(true));
		m_shift->setKeyHeld(m_shiftKey);
		for(size_t i = 0; i < 4; ++i)
			m_slotButtons[i]->setKeyHeld(focused && juce::KeyPress::isKeyCurrentlyDown(g_slotKeys[i]));
	}

	void Panel::timerCallback()
	{
		m_lcd.repaint();
		updateHeldKeys();
		// The knobs follow the G1's own positions, which something else may have moved (the
		// plugin's host automation), except the one being turned by hand.
		for(size_t i = 0; i < m_knobs.size(); ++i)
			if(!m_knobs[i].isMouseButtonDown())
				m_knobs[i].setValue(m_mc.adc(g_knobAdc[i]), juce::dontSendNotification);
		if(m_knobDisplays[0].isVisible())
			for(uint32_t k = 0; k < 18; ++k)
				m_knobDisplays[k].set(m_knobMap.read(k));
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
		for(uint32_t d = 0; d < g1::g_dspCount; ++d)
			dsp << (s.dspFailed[d] ? "x" : s.dspOn[d] ? "o" : "-");
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
			+ (s.rawMidi.empty() ? juce::String() : "  |  raw: " + juce::String(s.rawMidi))
			+ (s.dspProblem.empty() ? juce::String() : "  |  " + juce::String(s.dspProblem)), juce::dontSendNotification);
	}
}
