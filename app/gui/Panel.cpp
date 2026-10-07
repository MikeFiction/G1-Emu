#include "Panel.h"

#include "LcdFont.h"
#include "g1Lib/g1format.h"
#include "G1Skin.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace g1gui
{
	namespace
	{
		constexpr const auto& g_knobAdc = g1::KnobMap::KnobAdc;

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

		const juce::Colour g_face(Panel::FaceColour), g_textLight(0xffe8e8f0);
		constexpr int g_extrasHeight = 60;	// what the extras drawer adds below the panel
		constexpr float g_sectionsTop = 94.0f;	// the background's purple sections begin this far down

		// A rectangle in the background's pixels, in the panel's.
		juce::Rectangle<float> skf(const float _x, const float _y, const float _w, const float _h)
		{
			return juce::Rectangle<float>(_x, _y, _w, _h).transformedBy(juce::AffineTransform::scale(Panel::SkinScale));
		}
		juce::Rectangle<int> sk(const float _x, const float _y, const float _w, const float _h)
		{
			return skf(_x, _y, _w, _h).toNearestInt();
		}

		// A component holding a sprite gets whole pixels; the sprite keeps its exact place inside
		// them (a knob a pixel off its ring shows against the ticks).
		void place(juce::Component& _c, const juce::Rectangle<float> _exact)
		{
			const auto bounds = _exact.getSmallestIntegerContainer();
			_c.setBounds(bounds);
			auto& p = _c.getProperties();
			p.set("spriteX", _exact.getX() - static_cast<float>(bounds.getX()));
			p.set("spriteY", _exact.getY() - static_cast<float>(bounds.getY()));
			p.set("spriteW", _exact.getWidth());
			p.set("spriteH", _exact.getHeight());
		}

		juce::Rectangle<float> spriteArea(const juce::Component& _c)
		{
			const auto& p = _c.getProperties();
			if(!p.contains("spriteW"))
				return _c.getLocalBounds().toFloat();
			return {static_cast<float>(p["spriteX"]), static_cast<float>(p["spriteY"]), static_cast<float>(p["spriteW"]), static_cast<float>(p["spriteH"])};
		}

		// Smaller in halves first, so that no step skips pixels, as mipmaps would.
		juce::Image scaledTo(juce::Image _img, const int _w, const int _h)
		{
			while(_img.getWidth() >= _w * 2 && _img.getHeight() >= _h * 2)
				_img = _img.rescaled(_img.getWidth() / 2, _img.getHeight() / 2, juce::Graphics::highResamplingQuality);
			return _img.getWidth() == _w && _img.getHeight() == _h ? _img : _img.rescaled(_w, _h, juce::Graphics::highResamplingQuality);
		}
	}

	// One PNG: a single image, or frames of the same size in rows (a knob's 128 positions, a
	// button up and down). Each frame is scaled once to the screen's pixels where it is drawn and
	// kept, so a knob turning only picks frames. Only the last few sizes are kept: resizing the
	// window passes through hundreds.
	class Sprite
	{
	public:
		Sprite(const void* _png, const int _size, const int _frameW, const int _frameH, const int _count)
			: Sprite(juce::ImageCache::getFromMemory(_png, _size), _frameW, _frameH, _count) {}

		Sprite(juce::Image _sheet, const int _frameW, const int _frameH, const int _count)
			: m_sheet(std::move(_sheet)), m_w(_frameW), m_h(_frameH), m_count(_count)
			, m_cols(std::max(1, m_sheet.getWidth() / _frameW))
		{
			jassert(m_sheet.isValid() && (m_count + m_cols - 1) / m_cols * m_h <= m_sheet.getHeight());
		}

		int count() const { return m_count; }
		juce::Point<int> frameSize() const { return {m_w, m_h}; }

		// With _tint, the frame's shape filled with that colour instead (it lights a held button).
		void draw(juce::Graphics& _g, const int _frame, const juce::Rectangle<float> _dest, const juce::Colour _tint = {}) const
		{
			const float scale = _g.getInternalContext().getPhysicalPixelScaleFactor();
			const int w = std::max(1, juce::roundToInt(_dest.getWidth() * scale)), h = std::max(1, juce::roundToInt(_dest.getHeight() * scale));
			const auto& img = frame(std::clamp(_frame, 0, m_count - 1), w, h);
			if(!_tint.isTransparent())
				_g.setColour(_tint);
			_g.drawImage(img, _dest, juce::RectanglePlacement::stretchToFit, !_tint.isTransparent());
		}

	private:
		static constexpr size_t MaxSizes = 3;	// a panel on two screens of different scales, and one more

		struct Sized
		{
			std::vector<juce::Image> frames;
			uint64_t used = 0;
		};

		const juce::Image& frame(const int _frame, const int _w, const int _h) const
		{
			auto it = m_cache.find({_w, _h});
			if(it == m_cache.end())
			{
				if(m_cache.size() >= MaxSizes)
					m_cache.erase(std::min_element(m_cache.begin(), m_cache.end(), [](const auto& _a, const auto& _b) { return _a.second.used < _b.second.used; }));
				it = m_cache.emplace(std::make_pair(_w, _h), Sized{std::vector<juce::Image>(static_cast<size_t>(m_count)), 0}).first;
			}
			it->second.used = ++m_clock;
			auto& img = it->second.frames[static_cast<size_t>(_frame)];
			if(img.isNull())
				img = scaledTo(m_sheet.getClippedImage({_frame % m_cols * m_w, _frame / m_cols * m_h, m_w, m_h}), _w, _h);
			return img;
		}

		juce::Image m_sheet;
		int m_w, m_h, m_count, m_cols;
		mutable std::map<std::pair<int, int>, Sized> m_cache;
		mutable uint64_t m_clock = 0;
	};

	namespace
	{
		// The dial's ridges are all alike, so a picture of it does not show it turning: it is
		// turned here, through one ridge in DialSteps frames, after which it looks as it started.
		// So little turning leaves the light on it where it was. The picture (skin/dial.png, the
		// first frame) is DialFrame pixels square with the knob's centre in its middle; DialRidges
		// must be its count, and the ridges evenly spaced, or the loop jumps where they are not.
		// The thumb indent shows the whole turn (DialView::paint): Mike Fiction's painted one, cut
		// out of the knob (skin/dial_indent.png, its centre at IndentX/Y in it) and slid round the
		// face without turning, so the light on it stays where it is.
		constexpr int DialSteps = 12, DialFrame = 236, DialRidges = 64;
		constexpr float IndentRadius = 58.5f;					// from the knob's centre, straight up as painted
		constexpr float IndentSize = 48, IndentX = 24, IndentY = 23.5f;

		juce::Image makeDialLoop()
		{
			const auto knob = juce::ImageCache::getFromMemory(G1Skin::dial_png, G1Skin::dial_pngSize).getClippedImage({0, 0, DialFrame, DialFrame});
			juce::Image sheet(juce::Image::ARGB, DialFrame, DialFrame * DialSteps, true);
			juce::Graphics g(sheet);
			g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
			constexpr float centre = DialFrame / 2.0f;
			for(int i = 0; i < DialSteps; ++i)
			{
				const float angle = juce::MathConstants<float>::twoPi / static_cast<float>(DialRidges) * static_cast<float>(i) / static_cast<float>(DialSteps);
				g.drawImageTransformed(knob, juce::AffineTransform::rotation(angle, centre, centre).translated(0.0f, static_cast<float>(DialFrame * i)));
			}
			return sheet;
		}
	}

	struct Skin
	{
		Sprite background{G1Skin::background_png, G1Skin::background_pngSize, 3000, 1238, 1};
		Sprite knob{G1Skin::knob_png, G1Skin::knob_pngSize, 113, 113, 128};			// 0: fully left
		Sprite dial{makeDialLoop(), DialFrame, DialFrame, DialSteps};					// one ridge's turn, a loop
		Sprite dialIndent{G1Skin::dial_indent_png, G1Skin::dial_indent_pngSize, 48, 48, 1};
		Sprite dialShadow{G1Skin::dial_shadow_png, G1Skin::dial_shadow_pngSize, 289, 289, 1};
		Sprite buttonWide{G1Skin::button_wide_png, G1Skin::button_wide_pngSize, 180, 90, 2};	// up, down
		Sprite buttonWideHeld{G1Skin::button_wide_held_png, G1Skin::button_wide_held_pngSize, 180, 90, 1};	// latched or held by a key
		Sprite buttonTall{G1Skin::button_tall_png, G1Skin::button_tall_pngSize, 90, 180, 2};
		Sprite buttonTilted{G1Skin::button_tilted_png, G1Skin::button_tilted_pngSize, 148, 148, 2};	// centred on the button
		Sprite buttonTiltedHeld{G1Skin::button_tilted_held_png, G1Skin::button_tilted_held_pngSize, 148, 148, 1};
		Sprite knobShadow{G1Skin::knob_shadow_png, G1Skin::knob_shadow_pngSize, 100, 101, 1};		// its disk's centre at 33, 33
		Sprite led{G1Skin::led_png, G1Skin::led_pngSize, 70, 70, 2};					// off, on
		Sprite smallLcd{G1Skin::small_lcd_png, G1Skin::small_lcd_pngSize, 170, 57, 1};				// a knob's parameter display
		Sprite smallLcdDark{G1Skin::small_lcd_dark_png, G1Skin::small_lcd_dark_pngSize, 170, 57, 1};	// the same, nothing assigned
	};

	namespace
	{
		// The panel holds the skin while it is open, so this only finds it.
		const Skin& skin() { return *juce::SharedResourcePointer<Skin>(); }

		const Sprite& buttonSprite(const PanelButton::Shape _shape)
		{
			switch(_shape)
			{
			case PanelButton::Shape::Tall:		return skin().buttonTall;
			case PanelButton::Shape::Tilted:	return skin().buttonTilted;
			default:							return skin().buttonWide;
			}
		}

		// Where each sprite goes, in the background's pixels (measured against the reference image).
		// The knobs are at 1:1, their pivot (56, 56 in the frame) on the centre of the background's
		// red ring, measured ring by ring: the rows are not quite straight.
		struct Centre { float x, y; };
		constexpr Centre g_masterKnob{177.5f, 276.5f};
		constexpr Centre g_knobs[18] = {
			{448.5f, 276.5f}, {448.5f, 516.5f}, {448.5f, 751.5f},		// 1-3
			{672.5f, 276.5f}, {672.5f, 516.5f}, {672.5f, 750.5f},		// 4-6
			{897.5f, 276.5f}, {897.5f, 516.5f}, {897.5f, 751.5f},		// 7-9
			{1121.5f, 276.5f}, {1121.5f, 516.5f}, {1121.5f, 750.5f},	// 10-12
			{1352.5f, 276.5f}, {1352.5f, 517.5f}, {1352.5f, 750.5f},	// 13-15
			{1582.5f, 277.5f}, {1582.5f, 517.5f}, {1582.5f, 751.5f}};	// 16-18
		constexpr float g_knobLedX[] = {517.5f, 741.5f, 966.5f, 1190.5f, 1421.5f, 1651.5f}, g_knobLedY[] = {194.5f, 435.5f, 667.5f};
		// The parameter displays, 170 x 57: centred on their knob, over its LED (which they replace
		// while shown), their bottom edge clear of the knob's number and ring.
		constexpr float g_knobLcdW = 170, g_knobLcdH = 57, g_knobLcdBottom[] = {208, 450, 681};
		// Their glass inside the artwork's frame, 2 lines of 11 characters, and its dots' size.
		constexpr int g_knobLcdCols = 11, g_knobLcdRows = 2;
		constexpr float g_knobLcdGlassW = g_knobLcdW * 0.92f, g_knobLcdGlassH = g_knobLcdH * 0.74f;
		constexpr float g_knobLcdDot = std::min(g_knobLcdGlassW / g_knobLcdCols / 6.0f, g_knobLcdGlassH / g_knobLcdRows / 9.0f);
		// The tooltips' display below the knobs: its glass, and one line of characters a little
		// larger than theirs.
		constexpr float g_tipX = 1225, g_tipY = 895, g_tipW = 482, g_tipH = 38;
		constexpr float g_tipDot = 2.9f;
		constexpr int g_tipCols = 26;
		constexpr float g_assignX = 2388, g_assignY = 453.5f;	// the tilted button's centre: as far from "Assign" as from "Morph"
		constexpr float g_modeX[] = {1798.5f, 1918.5f, 2037.5f, 2157.5f};	// the LEDs' centres; the buttons below
		constexpr float g_dialX = 2619, g_dialY = 580, g_dialSize = 233.8f;	// the knob (234 of 236) as large as before
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
		// The glass painted over the background's, whose blurred dots would not match these.
		const auto area = getLocalBounds().toFloat();
		const bool on = m_lcd.displayOn();
		_g.setColour(on ? juce::Colour(0xff6aa52d) : juce::Colour(0xff4b7620));	// the background's glass
		_g.fillRect(area);
		_g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.35f), 0, 0, juce::Colours::transparentBlack, 0, 5.0f, false));
		_g.fillRect(area);
		if(!on)
			return;
		const auto glass = area.reduced(6.0f, 5.0f);

		constexpr int cols = 16, rows = 2;
		const float cellW = glass.getWidth() / cols, cellH = glass.getHeight() / rows;
		const float dot = std::min(cellW / 6.0f, cellH / 9.0f);
		const auto cg = m_lcd.cgram();
		const auto& font = lcdFont();
		const auto ink = juce::Colour(0xff141d06), ghost = juce::Colour(0xff619b29);

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
		skin().led.draw(_g, m_on ? 1 : 0, spriteArea(*this));
	}

	PanelButton::PanelButton(const juce::String& _name, g1::Microcontroller& _mc, const MatrixBit _bit, const Shape _shape)
		: juce::Button(_name), m_mc(_mc), m_bit(_bit), m_shape(_shape)
	{
		setLcdTip(*this, _name);
		if(!_bit.known())
		{
			setEnabled(false);
			setTooltip("Not yet identified in the panel matrix");
			return;
		}
		setTooltip("Right click: hold it down");
		setWantsKeyboardFocus(false);	// the keys are the panel's: see Panel::keyPressed
		onStateChange = [this] { update(); };
	}

	namespace
	{
		// When the held buttons' pulse started: one for all of them, so they glow together, and
		// restarted (lit) whenever another one is held.
		juce::uint32 g_pulseStart = 0;
	}

	void PanelButton::update()
	{
		if(held() && !m_wasHeld)
			g_pulseStart = juce::Time::getMillisecondCounter();
		m_wasHeld = held();
		const bool down = isDown() || held();
		if(down != m_down)
		{
			m_down = down;
			m_repeatUp = false;
			if(down)
				m_taken = takesPress && takesPress();
			if(!m_taken)
				setBit(down);
			m_pressedAt = juce::Time::getMillisecondCounter();
			if(const auto& callback = down ? onPress : onRelease)
				callback();
		}
		// The timer repeats a navigator key held with the mouse, and pulses a button held without it.
		if(!timerNeeded())
			stopTimer();
		else if(!isTimerRunning())
			startTimer(15);
		repaint();
	}

	void PanelButton::setBit(const bool _down)
	{
		m_mc.setButton(static_cast<uint32_t>(m_bit.row), static_cast<uint32_t>(m_bit.bit), _down);
	}

	namespace
	{
		// The repeat: after half a second held, let go for 40 ms and press for 80, about 8 presses
		// a second. Half a second, as a computer keyboard waits: a third caught slow clicks, which
		// moved two steps. The let-go must last long enough for the OS's scan of the panel to see
		// it: if presses get lost, lengthen g_repeatUpMs.
		constexpr juce::uint32 g_repeatDelayMs = 500, g_repeatUpMs = 40, g_repeatPeriodMs = 120;
		// A held button's glow: from dark to lit and back, once in this long.
		constexpr double g_pulsePeriodMs = 1200.0;
		// How long the G1 sees Shift let go after Shift + Random, for the OS to take the knobs.
		constexpr juce::uint32 g_shiftAfterRandomMs = 300;
		// A knob the window has just turned (by hand, Random) shows its own position this long
		// before it follows the patch again: until the OS has read it, the patch's value is the old.
		constexpr juce::uint32 g_knobSettleMs = 500;
	}

	void PanelButton::setAutoRepeat(const bool _on)
	{
		m_autoRepeat = _on;
		setLatchable(!_on);
		if(isEnabled() && _on)
			setTooltip("Hold: repeats");
	}

	void PanelButton::setLatchable(const bool _on)
	{
		m_latchable = _on;
		if(isEnabled() && !m_autoRepeat)
			setTooltip(_on ? "Right click: hold it down" : "");
	}

	void PanelButton::pressAgain(const juce::uint32 _upMs)
	{
		if(!m_down)
			return;
		setBit(false);
		juce::Timer::callAfterDelay(static_cast<int>(_upMs), [b = juce::Component::SafePointer<PanelButton>(this), id = ++m_pressAgainId]
		{
			if(b != nullptr && b->m_down && b->m_pressAgainId == id)
				b->setBit(true);
		});
	}

	void PanelButton::timerCallback()
	{
		if(held())
			repaint();	// the pulse
		if(repeating())
			repeat();
		if(!timerNeeded())
			stopTimer();
	}

	void PanelButton::repeat()
	{
		const auto since = juce::Time::getMillisecondCounter() - m_pressedAt;
		if(since < g_repeatDelayMs)
			return;
		const bool up = (since - g_repeatDelayMs) % g_repeatPeriodMs < g_repeatUpMs;
		if(up != m_repeatUp)
		{
			m_repeatUp = up;
			setBit(!up);
		}
	}

	// A right click (Ctrl+click on a Mac) latches the button instead of pressing it, so several
	// can be down at once: hold A, click B, and the G1 sees both. Not a key held for nothing (the
	// navigator's, which repeat, and Panel Split): latched it would only stick. A left click on a
	// latched button lets it go, and is not a press of its own.
	void PanelButton::mouseDown(const juce::MouseEvent& _e)
	{
		m_unlatching = false;
		if(_e.mods.isPopupMenu())
		{
			if(isEnabled() && m_latchable)
			{
				m_latched = !m_latched;
				update();
			}
			return;
		}
		if(!m_latched)
			return juce::Button::mouseDown(_e);
		m_unlatching = true;
		unlatch();
	}

	void PanelButton::mouseDrag(const juce::MouseEvent& _e)
	{
		if(!_e.mods.isPopupMenu() && !m_unlatching)
			juce::Button::mouseDrag(_e);
	}

	void PanelButton::mouseUp(const juce::MouseEvent& _e)
	{
		if(!_e.mods.isPopupMenu() && !std::exchange(m_unlatching, false))
			juce::Button::mouseUp(_e);
	}

	void PanelButton::paintButton(juce::Graphics& _g, const bool _over, const bool _down)
	{
		const auto& sprite = buttonSprite(m_shape);
		const auto r = spriteArea(*this);
		const int frame = _down || held() ? 1 : 0;
		// Held without the mouse (latched, or by a key): its own picture where the skin has one,
		// the body alone with no shadow, fading in and out over the down one, so it shows that the
		// G1 sees it down.
		const float opacity = isEnabled() ? 1.0f : 0.5f;
		_g.setOpacity(opacity);
		sprite.draw(_g, frame, r);
		const Sprite* heldPicture = m_shape == Shape::Wide ? &skin().buttonWideHeld : m_shape == Shape::Tilted ? &skin().buttonTiltedHeld : nullptr;
		if(held() && heldPicture)
		{
			const double t = static_cast<double>(juce::Time::getMillisecondCounter() - g_pulseStart) / g_pulsePeriodMs;
			const float glow = 0.5f + 0.5f * static_cast<float>(std::cos(t * juce::MathConstants<double>::twoPi));	// lit when it starts
			_g.setOpacity(opacity * glow);
			heldPicture->draw(_g, 0, r);
			_g.setOpacity(opacity);
		}
		if(_over && isEnabled() && !held())	// held, its glow is what it shows
			sprite.draw(_g, frame, r, juce::Colours::white.withAlpha(0.07f));
	}

	// The button itself, not its shadow nor the transparent rest of the frame: the frames of
	// neighbouring buttons overlap.
	bool PanelButton::hitTest(const int _x, const int _y)
	{
		const auto size = buttonSprite(m_shape).frameSize();
		const auto area = spriteArea(*this);
		if(area.isEmpty())
			return false;
		const float x = (static_cast<float>(_x) - area.getX()) * static_cast<float>(size.x) / area.getWidth();
		const float y = (static_cast<float>(_y) - area.getY()) * static_cast<float>(size.y) / area.getHeight();
		switch(m_shape)
		{
		case Shape::Wide:	return juce::Rectangle<float>(36, 23, 114, 61).contains(x, y);
		case Shape::Tall:	return juce::Rectangle<float>(21, 36, 60, 117).contains(x, y);
		case Shape::Tilted:	return juce::Point<float>(x, y).getDistanceFrom({74, 74}) < 56.0f;
		}
		return false;
	}

	namespace
	{
		// The icons, drawn on a 24 x 24 grid and scaled into the button.

		// Eight teeth and a ring, all overlapping: non-zero winding fills them as one. The hole is
		// painted over it by the button, in its own colour.
		juce::Path gearIcon()
		{
			juce::Path p;
			for(int i = 0; i < 8; ++i)
			{
				juce::Path tooth;
				tooth.addRoundedRectangle(10.5f, 1.5f, 3.0f, 5.0f, 0.8f);
				tooth.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::twoPi * static_cast<float>(i) / 8.0f, 12.0f, 12.0f));
				p.addPath(tooth);
			}
			p.addEllipse(4.5f, 4.5f, 15.0f, 15.0f);
			return p;
		}

		juce::Path warningIcon()
		{
			juce::Path p;
			p.startNewSubPath(12.0f, 2.0f);
			p.lineTo(23.0f, 21.5f);
			p.lineTo(1.0f, 21.5f);
			p.closeSubPath();
			p = p.createPathWithRoundedCorners(2.0f);
			p.setUsingNonZeroWinding(false);	// the exclamation mark is cut out of the triangle
			p.addRoundedRectangle(10.6f, 8.0f, 2.8f, 7.5f, 1.2f);
			p.addEllipse(10.5f, 16.8f, 3.0f, 3.0f);
			return p;
		}

		juce::Path patreonIcon()
		{
			juce::Path p;
			p.addEllipse(8.0f, 2.0f, 14.0f, 14.0f);
			p.addRectangle(2.0f, 2.0f, 4.0f, 20.0f);
			return p;
		}

		// A line drawn with a round pen, in the whole 24 x 24 frame (two hairlines at its sides),
		// so it scales as the filled icons do.
		juce::Path strokedIcon(const juce::Path& _line, const float _width)
		{
			juce::Path p;
			juce::PathStrokeType(_width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, _line);
			p.addRectangle(0.0f, 0.0f, 0.01f, 24.0f);
			p.addRectangle(23.99f, 0.0f, 0.01f, 24.0f);
			return p;
		}

		// A chevron: down opens the drawer, up closes it.
		juce::Path chevronIcon(const bool _down)
		{
			juce::Path line;
			line.startNewSubPath(3.0f, _down ? 8.0f : 16.0f);
			line.lineTo(12.0f, _down ? 17.0f : 7.0f);
			line.lineTo(21.0f, _down ? 8.0f : 16.0f);
			return strokedIcon(line, 3.2f);
		}

		// The power symbol: a ring open at the top, and the line through the gap.
		juce::Path powerIcon()
		{
			juce::Path line;
			line.addCentredArc(12.0f, 13.0f, 9.0f, 9.0f, 0.0f, juce::degreesToRadians(35.0f), juce::degreesToRadians(325.0f), true);
			line.startNewSubPath(12.0f, 1.5f);
			line.lineTo(12.0f, 11.0f);
			return strokedIcon(line, 3.0f);
		}

		juce::Path iconPath(const IconButton::Icon _icon)
		{
			switch(_icon)
			{
			case IconButton::Icon::Settings:	return gearIcon();
			case IconButton::Icon::Report:		return warningIcon();
			case IconButton::Icon::Patreon:		return patreonIcon();
			case IconButton::Icon::ExtrasOpen:	return chevronIcon(true);
			case IconButton::Icon::ExtrasClose:	return chevronIcon(false);
			case IconButton::Icon::Restart:		return powerIcon();
			}
			return {};
		}
	}

	void IconButton::paintButton(juce::Graphics& _g, const bool _over, const bool _down)
	{
		auto r = getLocalBounds().toFloat().reduced(1.0f);
		if(_down)
			r = r.translated(0, 1.0f);
		_g.setColour(juce::Colour(0xff1b1b1e).brighter(_over ? 0.35f : 0.15f));
		_g.fillRoundedRectangle(r, 4.0f);

		const auto p = iconPath(m_icon);
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

	namespace
	{
		constexpr float g_detentDegrees = 7.5f;		// 48 detents a turn
		// The most the picture turns in a tick (1/60 s): under half a ridge, or the ridges would
		// seem to turn backwards, as a film's wagon wheels do.
		constexpr float g_maxDegreesPerTick = 2.5f;
	}

	// One detent every 8 pixels of drag, or one per wheel click; the dial turns with it, a few
	// degrees a tick, until it is where the detents put it.
	void DialView::turn(const int _detents)
	{
		if(!_detents)
			return;
		m_mc.turnDial(_detents);
		m_targetAngle += static_cast<float>(_detents) * g_detentDegrees;
		if(!isTimerRunning())
			startTimerHz(60);
	}

	void DialView::timerCallback()
	{
		const float left = m_targetAngle - m_angle;
		if(std::abs(left) < 0.01f)
		{
			m_angle = m_targetAngle;
			stopTimer();
			repaint();
			return;
		}
		// Turned fast, it catches up within a tenth of a second rather than lagging behind.
		const float step = std::min(std::abs(left), std::max(g_maxDegreesPerTick, std::abs(left) / 6.0f));
		m_angle += left > 0 ? step : -step;
		repaint();
	}

	// The knob is round: only it takes the mouse, not the corners of its frame.
	bool DialView::hitTest(const int _x, const int _y)
	{
		const auto area = spriteArea(*this);
		return area.getCentre().getDistanceFrom({static_cast<float>(_x), static_cast<float>(_y)}) < area.getHeight() * 0.5f;
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

	// The knob at its angle: the ridges from the loop (where they are within a ridge), and the
	// thumb indent on its face, which goes round with the whole turn. The light stays top left.
	void DialView::paint(juce::Graphics& _g)
	{
		const auto area = spriteArea(*this);
		const float pitch = 360.0f / static_cast<float>(DialRidges);
		const float phase = std::fmod(std::fmod(m_angle, pitch) + pitch, pitch) / pitch;	// 0..1 of a ridge
		skin().dial.draw(_g, juce::roundToInt(phase * static_cast<float>(DialSteps)) % DialSteps, area);

		// The indent at the angle turned, from straight up where it was painted.
		const float scale = area.getWidth() / static_cast<float>(DialFrame);
		const float a = juce::degreesToRadians(m_angle);
		const auto centre = area.getCentre();
		const juce::Point<float> c(centre.x + std::sin(a) * IndentRadius * scale, centre.y - std::cos(a) * IndentRadius * scale);
		skin().dialIndent.draw(_g, 0, {c.x - IndentX * scale, c.y - IndentY * scale, IndentSize * scale, IndentSize * scale});
	}

	// The cap with its pointer; the red ring and its ticks are the background's.
	void KnobLook::drawRotarySlider(juce::Graphics& _g, const int _x, const int _y, const int _w, const int _h, const float _pos, float, float, juce::Slider& _slider)
	{
		const auto& knob = skin().knob;
		const int frame = juce::roundToInt(_pos * static_cast<float>(knob.count() - 1));
		const auto area = _slider.getProperties().contains("spriteW") ? spriteArea(_slider) : juce::Rectangle<int>(_x, _y, _w, _h).toFloat();
		knob.draw(_g, frame, area);

		// Excluded from Random: a small padlock at the knob's lower right.
		if(_slider.getProperties()["randomExcluded"])
		{
			const float r = area.getWidth() * 0.13f;
			const auto badge = juce::Rectangle<float>(2 * r, 2 * r).withCentre({area.getRight() - r, area.getBottom() - r});
			_g.setColour(juce::Colours::black.withAlpha(0.7f));
			_g.fillEllipse(badge);
			const auto body = juce::Rectangle<float>(r * 0.9f, r * 0.7f).withCentre(badge.getCentre().translated(0, r * 0.22f));
			juce::Path shackle;
			shackle.addCentredArc(body.getCentreX(), body.getY(), r * 0.3f, r * 0.36f, 0, -juce::MathConstants<float>::halfPi,
				juce::MathConstants<float>::halfPi, true);
			_g.setColour(juce::Colours::white.withAlpha(0.9f));
			_g.strokePath(shackle, juce::PathStrokeType(r * 0.16f));
			_g.fillRoundedRectangle(body, r * 0.12f);
		}
	}

	// ________________________________________________________________________

	namespace
	{
		// A line in the big display's dots (LcdFont.h), 5 x 7 a character, in cells _cellW wide from
		// _x, _y their top. Only the lit dots: the glass is the artwork's.
		void drawLcdText(juce::Graphics& _g, const juce::String& _line, const float _x, const float _y, const float _cellW, const float _dot)
		{
			const auto& font = lcdFont();
			_g.setColour(juce::Colour(0xff141d06));
			for(int c = 0; c < _line.length(); ++c)
			{
				const auto ch = static_cast<juce::juce_wchar>(_line[c]);
				if(ch <= 0x20 || ch >= 0x80)
					continue;
				const float x0 = _x + static_cast<float>(c) * _cellW + (_cellW - _dot * 5.0f) * 0.5f;
				for(int y = 0; y < 7; ++y)
					for(int x = 0; x < 5; ++x)
						if((font[static_cast<size_t>(ch - 0x20)][static_cast<size_t>(x)] >> y) & 1)
							_g.fillRect(x0 + static_cast<float>(x) * _dot, _y + static_cast<float>(y) * _dot, _dot * 0.86f, _dot * 0.86f);
			}
		}

		// The display's steps, 150 ms each. Scrolling: a character a step, after the start and the
		// end have shown for 10 steps. A flash: shown for 2 steps and blank for 1, twice.
		constexpr int g_tipStepMs = 150, g_tipHoldSteps = 10;
		constexpr int g_flashOnSteps = 2, g_flashPeriod = 3, g_flashSteps = 2 * g_flashPeriod;

		// The floating tooltips' look.
		constexpr float g_tipFontHeight = 12.0f, g_tipCorner = 5.0f, g_tipBlur = 6.0f;
		constexpr int g_tipPadX = 8, g_tipPadY = 5;

		juce::TextLayout tipLayout(const juce::String& _tip)
		{
			juce::AttributedString s;
			s.setJustification(juce::Justification::centredLeft);
			s.append(_tip, juce::Font(juce::FontOptions(g_tipFontHeight)), juce::Colours::white);
			juce::TextLayout layout;
			layout.createLayoutWithBalancedLineLengths(s, 400.0f);
			return layout;
		}
	}

	// As JUCE's own: beside the pointer, towards the middle of the panel.
	juce::Rectangle<int> TooltipLook::getTooltipBounds(const juce::String& _tip, const juce::Point<int> _pos, const juce::Rectangle<int> _parentArea)
	{
		const auto layout = tipLayout(_tip);
		const int w = static_cast<int>(std::ceil(layout.getWidth())) + 2 * g_tipPadX;
		const int h = static_cast<int>(std::ceil(layout.getHeight())) + 2 * g_tipPadY;
		return juce::Rectangle<int>(_pos.x > _parentArea.getCentreX() ? _pos.x - (w + 12) : _pos.x + 24,
			_pos.y > _parentArea.getCentreY() ? _pos.y - (h + 6) : _pos.y + 6, w, h).constrainedWithin(_parentArea);
	}

	void TooltipLook::drawTooltip(juce::Graphics& _g, const juce::String& _tip, const int _w, const int _h)
	{
		if(m_snapshotting)
			return;
		const juce::Rectangle<float> area(0.0f, 0.0f, static_cast<float>(_w), static_cast<float>(_h));
		juce::Path glass;
		glass.addRoundedRectangle(area, g_tipCorner);

		// What is behind it, blurred: the panel drawn without the tooltip, in the screen's pixels.
		if(auto* panel = m_window.getParentComponent())
		{
			const float scale = _g.getInternalContext().getPhysicalPixelScaleFactor();
			juce::Image behind;
			{
				const juce::ScopedValueSetter<bool> hidden(m_snapshotting, true);
				behind = panel->createComponentSnapshot(m_window.getBoundsInParent(), true, scale);
			}
			const float radius = g_tipBlur * scale;
			juce::ImageConvolutionKernel blur(2 * static_cast<int>(std::ceil(radius)) + 1);
			blur.createGaussianBlur(radius);
			const auto sharp = behind.createCopy();
			blur.applyToImage(behind, sharp, behind.getBounds());
			juce::Graphics::ScopedSaveState state(_g);
			_g.reduceClipRegion(glass);
			_g.drawImage(behind, area);
		}
		_g.setColour(juce::Colours::black.withAlpha(0.4f));
		_g.fillPath(glass);
		tipLayout(_tip).draw(_g, area.reduced(static_cast<float>(g_tipPadX), static_cast<float>(g_tipPadY)));
	}

	void setLcdTip(juce::Component& _c, const juce::String& _tip)
	{
		_c.getProperties().set("lcdTip", _tip);
	}

	void TipDisplay::flash(const juce::String& _text)
	{
		m_flash = _text;
		m_flashSteps = g_flashSteps;
		updateTimer();
		repaint();
	}

	void TipDisplay::set(const juce::String& _tip)
	{
		if(_tip == m_tip)
			return;
		m_tip = _tip;
		m_text = _tip.upToFirstOccurrenceOf("\t", false, false).trimEnd();
		m_value = _tip.fromFirstOccurrenceOf("\t", false, false).trim();
		m_scroll = m_ticks = 0;
		updateTimer();
		repaint();
	}

	bool TipDisplay::scrolls() const
	{
		return m_value.isEmpty() && m_text.length() > g_tipCols;
	}

	void TipDisplay::updateTimer()
	{
		if(m_flashSteps > 0 || scrolls())
			startTimer(g_tipStepMs);
		else
			stopTimer();
	}

	// A flash goes first; the scrolling waits for it.
	void TipDisplay::timerCallback()
	{
		repaint();
		if(m_flashSteps > 0)
		{
			if(--m_flashSteps == 0)
				updateTimer();
			return;
		}
		const int end = m_text.length() - g_tipCols;
		++m_ticks;
		if(m_scroll == 0 && m_ticks < g_tipHoldSteps)
			return;
		if(m_scroll < end)
			++m_scroll;
		else if(m_ticks >= end + 2 * g_tipHoldSteps)
			m_scroll = m_ticks = 0;
	}

	juce::String TipDisplay::line() const
	{
		if(m_flashSteps > 0)
			return (g_flashSteps - m_flashSteps) % g_flashPeriod < g_flashOnSteps ? m_flash.substring(0, g_tipCols) : juce::String();
		if(m_value.isEmpty())
			return m_text.substring(m_scroll, m_scroll + g_tipCols);
		// The value whole at the right, the description cut short before it if it must be.
		const int room = std::max(0, g_tipCols - m_value.length() - 1);
		return m_text.substring(0, room).trimEnd().paddedRight(' ', g_tipCols - m_value.length()) + m_value;
	}

	void TipDisplay::paint(juce::Graphics& _g)
	{
		const auto r = spriteArea(*this);
		const float dot = g_tipDot * r.getWidth() / g_tipW, cell = dot * 6.0f;
		drawLcdText(_g, line(), r.getCentreX() - cell * g_tipCols * 0.5f, r.getCentreY() - dot * 3.5f, cell, dot);
	}

	void KnobDisplay::set(const g1::KnobInfo& _info)
	{
		const juce::String top = _info.assigned ? juce::String(_info.moduleName) : juce::String();
		juce::String bottom = _info.assigned ? juce::String(_info.paramName) : juce::String();
		// The value as the editor reads it ("Sine", "1.25kHz"), without spaces: there are 11 characters.
		juce::String value = _info.assigned ? juce::String(g1::formatValue(_info.type, _info.param, _info.value)).removeCharacters(" ") : juce::String();
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
		const auto r = spriteArea(*this);
		(m_assigned ? skin().smallLcd : skin().smallLcdDark).draw(_g, 0, r);
		if(!m_assigned)
			return;	// unlit: the knob moves nothing
		const auto glass = r.withSizeKeepingCentre(r.getWidth() * g_knobLcdGlassW / g_knobLcdW, r.getHeight() * g_knobLcdGlassH / g_knobLcdH);
		const float cellW = glass.getWidth() / g_knobLcdCols, cellH = glass.getHeight() / g_knobLcdRows;
		const float dot = g_knobLcdDot * r.getWidth() / g_knobLcdW;
		const juce::String lines[g_knobLcdRows] = {m_top, m_bottom};
		for(int row = 0; row < g_knobLcdRows; ++row)
			drawLcdText(_g, lines[row].substring(0, g_knobLcdCols), glass.getX(), glass.getY() + static_cast<float>(row) * cellH + (cellH - dot * 7.0f) * 0.5f, cellW, dot);
	}

	Panel::Panel(PanelHost& _host) : m_host(_host), m_mc(_host.mc()), m_lcd(_host.mc().getLcd()), m_dial(_host.mc()), m_synthView(_host.synthSettings()), m_knobMap(_host.mc())
	{
		addAndMakeVisible(m_lcd);
		addAndMakeVisible(m_dial);
		addAndMakeVisible(m_tip);
		m_tooltips.setOpaque(false);	// rounded, over the panel blurred
		m_tooltips.setLookAndFeel(&m_tooltipLook);

		// Each knob starts where the G1's ADC says it is: the G1 is already running (and in the
		// plugin, the editor comes and goes), so writing a position here would be turning it.
		// _turnedAt, if given, keeps when the window last turned it (updateKnobs).
		auto setupKnob = [this](juce::Slider& _s, const uint8_t _adc, const juce::String& _tip, juce::uint32* _turnedAt)
		{
			_s.setLookAndFeel(&m_knobLook);
			_s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
			_s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
			_s.setRange(0, 255, 1);
			_s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
			setLcdTip(_s, _tip);
			_s.setValue(m_mc.adc(_adc), juce::dontSendNotification);
			_s.onValueChange = [this, &_s, _adc, _turnedAt]
			{
				m_mc.setAdc(_adc, static_cast<uint8_t>(_s.getValue()));
				if(_turnedAt)
					*_turnedAt = juce::Time::getMillisecondCounter();
			};
			addAndMakeVisible(_s);
		};
		setupKnob(m_volume, VolumeAdc, "Master Volume", nullptr);
		for(size_t i = 0; i < m_knobs.size(); ++i)
		{
			setupKnob(m_knobs[i], g_knobAdc[i], "Knob " + juce::String(static_cast<int>(i + 1)), &m_knobTurnedAt[i]);
			m_knobs[i].addMouseListener(&m_knobMenu, false);
			m_knobLeds[i] = &addLed({static_cast<int>(i % 3), 1 + static_cast<int>(i / 3)});
		}

		m_midiLed = &addLed({});
		m_panelSplitLed = &addLed(g_panelSplitLed);
		m_panelSplit = &addButton("Panel Split", g_btnPanelSplit);
		m_panelSplit->setLatchable(false);	// a toggle: held, it does nothing more
		m_find = &addButton("Find", g_btnFind);

		const char* modes[] = {"Store", "System", "Edit", "Patch/Load"};
		const MatrixBit modeBits[] = {g_btnStore, g_btnSystem, g_btnEdit, g_btnPatchLoad};
		for(size_t i = 0; i < 4; ++i)
		{
			m_modeButtons[i] = &addButton(modes[i], modeBits[i]);
			m_modeLeds[i] = &addLed(g_modeLeds[i]);
		}
		// The mode keys act on the press: held, they change nothing (the manual, NOTES.md)
		for(auto* b : m_modeButtons)
			b->setLatchable(false);
		const char* slots[] = {"A", "B", "C", "D"};
		const MatrixBit slotBits[] = {g_btnA, g_btnB, g_btnC, g_btnD};
		for(size_t i = 0; i < 4; ++i)
		{
			m_slotButtons[i] = &addButton(slots[i], slotBits[i]);
			setLcdTip(*m_slotButtons[i], juce::String("Slot ") + slots[i]);
			m_slotLeds[i] = &addLed(g_slotLeds[i]);
		}
		m_assign = &addButton("Assign / Morph", g_btnAssign, PanelButton::Shape::Tilted);
		m_shift = &addButton("Shift", g_btnShift);
		m_shiftNames = {
			{m_find, [](const bool _shift) { return juce::String(_shift ? "Panic" : "Find"); }},
			{m_modeButtons[0], [this](const bool _shift) { return storeName(_shift); }},
			{m_assign, [](const bool _shift) { return juce::String(_shift ? "Morph" : "Assign"); }},
			{m_modeButtons[3], [](const bool _shift) { return juce::String(_shift ? "Random Knobs" : "Patch/Load"); }}};
		// Shift + Patch/Load is the extras' Random: the OS has nothing of its own there (it asks
		// Load? as without Shift), so the G1 does not see that press. Shift stays held, for
		// another Random or another key.
		m_modeButtons[3]->takesPress = [this]
		{
			if(!m_shift->isPressed())
				return false;
			randomizeKnobs();
			return true;
		};
		m_nav[0] = &addButton("Up", g_btnUp, PanelButton::Shape::Tall);
		m_nav[1] = &addButton("Left", g_btnLeft);
		m_nav[2] = &addButton("Right", g_btnRight);
		m_nav[3] = &addButton("Down", g_btnDown, PanelButton::Shape::Tall);
		for(auto* nav : m_nav)
		{
			nav->setAutoRepeat(true);
			setLcdTip(*nav, "Navigator " + nav->getName());
		}

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
			setExtrasOpen(!m_extrasOpen, true);
			m_host.setExtrasOpen(m_extrasOpen);
		};
		addAndMakeVisible(m_extras);
		m_drawerClip.setInterceptsMouseClicks(false, true);
		m_drawerClip.addAndMakeVisible(m_drawer);
		addChildComponent(m_drawerClip);

		// Random: each of the 18 knobs to a value of its own, as if turned by hand. What it
		// changes is whatever the patch has assigned to them; knobs with nothing assigned do nothing.
		// Out of the extras for now: Shift + Patch/Load does it on the panel (randomizeKnobs).
		// m_random.setTooltip("Turn the 18 knobs to random positions (double click: back to the patch's values)");
		// m_random.onSingleClick = [this] { randomizeKnobs(); };
		// m_random.onDoubleClick = [this] { restoreKnobs(); };
		// m_drawer.addAndMakeVisible(m_random);

		m_displaysToggle.setTooltip("Show above each knob the module and parameter it moves");
		m_displaysToggle.setColour(juce::ToggleButton::textColourId, g_textLight);
		m_displaysToggle.onClick = [this]
		{
			setKnobDisplays(m_displaysToggle.getToggleState());
			m_host.setKnobDisplays(m_displaysToggle.getToggleState());
		};
		m_drawer.addAndMakeVisible(m_displaysToggle);
		for(auto& d : m_knobDisplays)
			addChildComponent(d);
		setKnobDisplays(m_host.knobDisplays());
		updateRandomExcluded();

		m_followToggle.setTooltip("Show each knob where the patch's value puts it, instead of where it was last turned");
		m_followToggle.setColour(juce::ToggleButton::textColourId, g_textLight);
		m_followToggle.setToggleState(m_host.knobFollowsPatch(), juce::dontSendNotification);
		m_followToggle.onClick = [this] { m_host.setKnobFollowsPatch(m_followToggle.getToggleState()); };
		m_drawer.addAndMakeVisible(m_followToggle);

		m_synthButton.setTooltip("The slots' MIDI channels, the clock and the other settings of the whole G1");
		m_synthButton.onClick = [this] { m_synthView.open(*this, FaceHeight, sk(0, g_sectionsTop, 3000, 1238 - g_sectionsTop)); };
		m_drawer.addAndMakeVisible(m_synthButton);

		// The power switch, where the host can work it: a G1 that hangs comes back without
		// closing G1-Emu. Asked first, since what is in the slots and not stored is lost.
		m_restart.setTooltip("Restart the G1");
		m_restart.onClick = [this]
		{
			m_confirm.open(*this, FaceHeight, sk(0, g_sectionsTop, 3000, 1238 - g_sectionsTop), "Restart the G1?",
				m_host.restartNote(), "Restart", [this] { m_host.restart(); });
		};
		if(m_host.canRestart())
			m_drawer.addAndMakeVisible(m_restart);
		m_confirm.onClose = [this] { grabKeyboardFocus(); };
		addChildComponent(m_confirm);
		m_synthView.onClose = [this] { grabKeyboardFocus(); };
		addChildComponent(m_synthView);

		setExtrasOpen(m_host.extrasOpen());

		// The computer's keyboard holds what a mouse cannot: Shift, and A-D together. The keys
		// reach the panel when it has the focus, which a click anywhere on it gives it.
		setWantsKeyboardFocus(true);
		addMouseListener(this, true);
		startTimerHz(30);
	}

	Panel::~Panel()
	{
		m_tooltips.setLookAndFeel(nullptr);
		m_volume.setLookAndFeel(nullptr);
		for(auto& k : m_knobs)
		{
			k.setLookAndFeel(nullptr);
			k.removeMouseListener(&m_knobMenu);
		}
	}

	PanelButton& Panel::addButton(const juce::String& _name, const MatrixBit _bit, const PanelButton::Shape _shape)
	{
		m_buttons.push_back(std::make_unique<PanelButton>(_name, m_mc, _bit, _shape));
		auto& b = *m_buttons.back();
		b.onPress = [this, &b] { buttonPressed(b); };
		b.onRelease = [this, &b] { buttonReleased(b); };
		addAndMakeVisible(b);
		return b;
	}

	void Panel::buttonPressed(PanelButton& _b)
	{
		if(&_b == m_shift || !m_shift->isPressed())
			return;
		m_shiftUsed = !_b.wasTaken();	// what the window takes (Random) leaves Shift held
		// Not Morph: it is held while the dial sets the range, a flash would only be in the way.
		for(const auto& s : m_shiftNames)
			if(const auto name = s.name(true); s.button == &_b && s.button != m_assign && name.isNotEmpty())
				m_tip.flash(name);
	}

	// Store asks "Store?" in Patch/Load mode only; Shift + Store saves the synth settings there
	// and in System. In Edit the OS ignores both (NOTES.md, "What each button does").
	juce::String Panel::storeName(const bool _shift) const
	{
		if(_shift)
			return m_mode == Mode::Patch || m_mode == Mode::System ? "Save Synth. Settings" : "";
		return m_mode == Mode::Patch ? "Store Patch" : "";
	}

	bool Panel::ledLit(const MatrixBit _bit) const
	{
		return !(m_mc.ledRow(static_cast<uint32_t>(_bit.row)) & (1u << _bit.bit));	// active low
	}

	// A mode LED counts as lit for a while after it was seen lit: in a prompt (Load?) it blinks.
	// Edit and System win over Patch/Load, Store's LED is not a mode of its own (Store? is asked
	// from Patch/Load, whose LED stays lit).
	void Panel::updateMode()
	{
		constexpr juce::uint32 holdMs = 600;
		const auto now = juce::Time::getMillisecondCounter();
		for(size_t i = 0; i < 4; ++i)
			if(ledLit(g_modeLeds[i]))
				m_modeLitAt[i] = now;
		const auto lit = [&](const size_t _i) { return m_modeLitAt[_i] != 0 && now - m_modeLitAt[_i] < holdMs; };
		m_mode = lit(2) ? Mode::Edit : lit(1) ? Mode::System : lit(3) ? Mode::Patch : Mode::Other;
	}

	void Panel::buttonReleased(PanelButton& _b)
	{
		if(&_b == m_shift || !std::exchange(m_shiftUsed, false))
			return;
		// In Edit mode Shift with the navigator walks from module to module, as many steps as
		// wanted with Shift held all along (NOTES.md, "The panel"): it stays down.
		updateMode();
		if(m_mode == Mode::Edit && std::find(m_nav.begin(), m_nav.end(), &_b) != m_nav.end())
			return;
		// The slot buttons take Shift one after the other (Shift, A, B: both slots' voices), so it
		// stays down for them. The OS still wants it pressed again for the next key: it gets that.
		if(std::find(m_slotButtons.begin(), m_slotButtons.end(), &_b) != m_slotButtons.end())
			return m_shift->pressAgain(g_repeatUpMs);
		if(m_shiftKey)
			m_shiftSpent = true;
		m_shift->unlatch();
		updateHeldKeys();
	}

	LedView& Panel::addLed(const MatrixBit _bit)
	{
		m_leds.push_back(std::make_unique<LedView>());
		addAndMakeVisible(*m_leds.back());
		if(_bit.known())
			m_ledMap.emplace_back(m_leds.back().get(), _bit);
		return *m_leds.back();
	}

	// The background carries the faceplate with every label, the knobs' red rings and the
	// display's frame; the rest are sprites over it.
	void Panel::paint(juce::Graphics& _g)
	{
		_g.fillAll(g_face);
		const auto& s = *m_skin;
		s.background.draw(_g, 0, {0.0f, 0.0f, static_cast<float>(Width), 1238.0f * SkinScale});
		s.dialShadow.draw(_g, 0, skf(g_dialX - 136.5f, g_dialY + 8.0f - 124.5f, 289, 289));
		// Under each knob, the shadow's disk under the cap: only its soft edge shows, down and right.
		const auto shadowAt = [&](const Centre _c) { s.knobShadow.draw(_g, 0, skf(_c.x - 33, _c.y - 33, 100, 101)); };
		shadowAt(g_masterKnob);
		for(const auto& c : g_knobs)
			shadowAt(c);
	}

	void ExtrasDrawer::paint(juce::Graphics& _g)
	{
		const auto tray = getLocalBounds().toFloat().reduced(12.0f, 0.0f).withTrimmedTop(5.0f).withTrimmedBottom(7.0f);
		_g.setColour(g_face.brighter(0.15f));
		_g.fillRoundedRectangle(tray, 10.0f);
		_g.setColour(g_textLight);
		_g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
		_g.drawText("EXTRAS", tray.toNearestInt().withWidth(110), juce::Justification::centred, false);
	}

	void Panel::resized()
	{
		// Everything in the background's pixels: a sprite's whole frame, centred where the
		// reference image has it.
		auto knobAt = [](const Centre _c) { return skf(_c.x - 56, _c.y - 56, 113, 113); };
		auto ledAt = [](const float _x, const float _y) { return skf(_x - 35, _y - 35, 70, 70); };
		auto wideAt = [](const float _x, const float _y) { return skf(_x - 36, _y - 23, 180, 90); };	// by the button's top left
		auto tallAt = [](const float _x, const float _y) { return skf(_x - 21, _y - 36, 90, 180); };

		// Left column
		place(m_volume, knobAt(g_masterKnob));
		place(*m_panelSplitLed, ledAt(142, 401));
		place(*m_panelSplit, wideAt(123, 424));	// centred (177) under "Find" and Master Volume
		place(*m_find, wideAt(123, 531));
		place(*m_midiLed, ledAt(142, 673));

		// Knobs: 1-3 and 4-6 in the first group, 7-12 in the second, 13-15 and 16-18 in the others
		for(size_t i = 0; i < m_knobs.size(); ++i)
		{
			place(m_knobs[i], knobAt(g_knobs[i]));
			place(*m_knobLeds[i], ledAt(g_knobLedX[i / 3], g_knobLedY[i % 3]));
			place(m_knobDisplays[i], skf(g_knobs[i].x - g_knobLcdW / 2, g_knobLcdBottom[i % 3] - g_knobLcdH, g_knobLcdW, g_knobLcdH));
		}

		// Right: display, modes, slots, navigator, Assign/Morph, Shift and the dial
		m_lcd.setBounds(sk(1781, 188, 477, 120));
		place(m_tip, skf(g_tipX, g_tipY, g_tipW, g_tipH));
		for(size_t i = 0; i < 4; ++i)
		{
			place(*m_modeLeds[i], ledAt(g_modeX[i], 474.5f));
			place(*m_modeButtons[i], wideAt(g_modeX[i] - 13.5f, 496));
			place(*m_slotLeds[i], ledAt(g_modeX[i], 705.5f));
			place(*m_slotButtons[i], wideAt(g_modeX[i] - 13.5f, 727));
		}
		// Up and down as far apart as the slot buttons (their pitch less the 114 of a button:
		// 5.67), the gap on the middle of left and right (281 + 61 / 2).
		constexpr float navGap = 119.67f - 114.0f, navMiddle = 281.0f + 61.0f / 2.0f;
		place(*m_nav[0], tallAt(2587, navMiddle - navGap / 2.0f - 117.0f));
		place(*m_nav[1], wideAt(2465, 281));
		place(*m_nav[2], wideAt(2655, 281));
		place(*m_nav[3], tallAt(2587, navMiddle + navGap / 2.0f));
		place(*m_assign, skf(g_assignX - 74, g_assignY - 74, 148, 148));
		place(*m_shift, wideAt(2323, 614));
		place(m_dial, skf(g_dialX - g_dialSize / 2, g_dialY - g_dialSize / 2, g_dialSize, g_dialSize));

		// The status bar below the face, with Extras, Patreon, Report issue and Settings at its right
		const int bar = FaceHeight + 3;
		m_status.setBounds(14, bar, getWidth() - 28 - 4 * 32, 30);
		m_extras.setBounds(getWidth() - 14 - 26 - 96, bar + 3, 26, 24);
		m_patreon.setBounds(getWidth() - 14 - 26 - 64, bar + 3, 26, 24);
		m_report.setBounds(getWidth() - 14 - 26 - 32, bar + 3, 26, 24);
		m_settings.setBounds(getWidth() - 14 - 26, bar + 3, 26, 24);
		m_drawerClip.setBounds(0, Height, getWidth(), g_extrasHeight);
		m_drawer.setSize(getWidth(), g_extrasHeight);
		// m_random.setBounds(140, 15, 100, 28);	// out of the extras for now
		m_synthButton.setBounds(140, 15, 110, 28);	// where Random was
		m_displaysToggle.setBounds(260, 15, 180, 28);
		m_followToggle.setBounds(450, 15, 200, 28);
		// At the right: Restart (where there is one).
		const int right = getWidth() - 12 - 10;	// the tray's edge, less the margin above and below them
		m_restart.setBounds(right - 28, 15, 28, 28);
		if(m_synthView.isVisible())
			m_synthView.setBounds(getLocalBounds());
		placeDrawer();
	}

	// The window grows at once and the drawer slides into the room (or out of it, and then the
	// window shrinks): resizing the window itself frame by frame would stutter in some hosts.
	void Panel::setExtrasOpen(const bool _open, const bool _animate)
	{
		m_extrasOpen = _open;
		m_extras.setIcon(_open ? IconButton::Icon::ExtrasClose : IconButton::Icon::ExtrasOpen);
		m_extras.setTooltip(_open ? "Hide the extras" : "Extras: knob displays, synth settings and more");	// was "Extras: Random and more"
		if(_open)
		{
			m_drawerClip.setVisible(true);
			setSize(Width, Height + g_extrasHeight);	// the window follows its content
		}
		if(_animate && isShowing())
		{
			m_slideFrom = m_drawerShown;
			m_slideStart = -1.0;
			m_slide.emplace(this, [this](const double _now) { slideDrawer(_now); });
			return;
		}
		m_slide.reset();
		m_drawerShown = _open ? 1.0f : 0.0f;
		placeDrawer();
		if(!_open)
		{
			m_drawerClip.setVisible(false);
			setSize(Width, Height);
		}
	}

	void Panel::slideDrawer(const double _now)
	{
		constexpr double duration = 0.075;	// seconds for the whole way
		if(m_slideStart < 0)
			m_slideStart = _now;
		const float target = m_extrasOpen ? 1.0f : 0.0f;
		const float t = std::min(1.0f, static_cast<float>((_now - m_slideStart) / (duration * std::abs(target - m_slideFrom) + 1e-6)));
		const float e = 1.0f - std::pow(1.0f - t, 3.0f);	// eases out: quick, then settles
		m_drawerShown = m_slideFrom + (target - m_slideFrom) * e;
		placeDrawer();
		// Done: the window shrinks if it closed. Not from inside the attachment's own callback.
		if(t >= 1.0f)
			juce::MessageManager::callAsync([p = juce::Component::SafePointer<Panel>(this)]
			{
				if(p != nullptr && p->m_slide)
					p->setExtrasOpen(p->m_extrasOpen);
			});
	}

	void Panel::placeDrawer()
	{
		m_drawer.setTopLeftPosition(0, juce::roundToInt(-(1.0f - m_drawerShown) * static_cast<float>(g_extrasHeight)));
	}

	void Panel::setKnobDisplays(const bool _on)
	{
		m_displaysToggle.setToggleState(_on, juce::dontSendNotification);
		for(auto& d : m_knobDisplays)
			d.setVisible(_on);
		// A display takes its knob's LED's place: lit or dark, it says what the LED would.
		for(auto* led : m_knobLeds)
			led->setVisible(!_on);
	}

	// The synth's control under the mouse, on the tooltips' display: at once, and also while it is
	// held, so a knob being turned shows its value. Blank anywhere else.
	void Panel::updateTip()
	{
		juce::String tip;
		const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
		for(auto* c = mouse.isTouch() ? nullptr : mouse.getComponentUnderMouse(); c != nullptr && isParentOf(c); c = c->getParentComponent())
		{
			if(const auto* t = c->getProperties().getVarPointer("lcdTip"))
			{
				tip = t->toString();
				break;
			}
		}
		m_tip.set(tip);
	}

	namespace
	{
		// Whether a knob moves the same parameter of the same module in both.
		bool sameAssignment(const g1::KnobInfo& _a, const g1::KnobInfo& _b)
		{
			return _a.assigned == _b.assigned && _a.slot == _b.slot && _a.section == _b.section
				&& _a.module == _b.module && _a.param == _b.param && _a.type == _b.type;
		}
	}

	// The OS ignores the knobs while Shift is down: when the window turns them (Random, its
	// double click) with Shift held, the G1 sees Shift let go while they move and down again after.
	// On the panel Shift stays as it was.
	void Panel::shiftAsideForKnobs()
	{
		if(m_shift->isPressed())
			m_shift->pressAgain(g_shiftAfterRandomMs);
	}

	void Panel::randomizeKnobs()
	{
		shiftAsideForKnobs();
		// Keep the patch's values the first time, and again once the assignments have changed.
		std::array<g1::KnobInfo, 18> now;
		bool same = m_haveSnapshot;
		for(uint32_t k = 0; k < 18; ++k)
		{
			now[k] = m_knobMap.read(k);
			same = same && sameAssignment(now[k], m_snapshot[k]);
		}
		if(!same)
		{
			m_snapshot = now;
			m_haveSnapshot = true;
		}
		const auto excluded = m_host.randomExcluded();
		for(size_t k = 0; k < m_knobs.size(); ++k)
			if(!(excluded & (1u << k)))
				m_knobs[k].setValue(1 + m_rng.nextInt(254), juce::sendNotificationSync);	// 0 and 255 the OS ignores
	}

	void Panel::KnobMenuListener::mouseDown(const juce::MouseEvent& _e)
	{
		if(!_e.mods.isPopupMenu())
			return;
		for(size_t k = 0; k < panel.m_knobs.size(); ++k)
			if(_e.eventComponent == &panel.m_knobs[k])
				panel.showKnobMenu(k);
	}

	// Some knobs are better left where they are when the rest go random: an output level, say.
	// Which ones is the host's to keep (a project keeps its own in the plugin).
	void Panel::showKnobMenu(const size_t _knob)
	{
		const auto excluded = m_host.randomExcluded();
		const auto bit = 1u << _knob;
		juce::PopupMenu menu;
		menu.addSectionHeader("Knob " + juce::String(static_cast<int>(_knob + 1)));
		menu.addItem("Exclude from Random", true, (excluded & bit) != 0, [this, bit]
		{
			m_host.setRandomExcluded(m_host.randomExcluded() ^ bit);
			updateRandomExcluded();
		});
		menu.addItem("Include all knobs in Random", excluded != 0, false, [this]
		{
			m_host.setRandomExcluded(0);
			updateRandomExcluded();
		});
		menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&m_knobs[_knob]));
	}

	void Panel::updateRandomExcluded()
	{
		const auto excluded = m_host.randomExcluded();
		for(size_t k = 0; k < m_knobs.size(); ++k)
		{
			const bool out = (excluded & (1u << k)) != 0;
			m_knobs[k].getProperties().set("randomExcluded", out);
			m_knobs[k].setTooltip(out ? "Excluded from Random (right click to include it)" : "Right click: exclude from Random");
			m_knobs[k].repaint();
		}
	}

	// Only where the knob still moves the same parameter, and not the morph groups (left to their
	// own positions for now).
	void Panel::restoreKnobs()
	{
		if(!m_haveSnapshot)
			return;
		shiftAsideForKnobs();
		for(uint32_t k = 0; k < 18; ++k)
		{
			const auto& s = m_snapshot[k];
			if(s.assigned && s.section != 2 && sameAssignment(s, m_knobMap.read(k)))
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
		if(!shift)
			m_shiftSpent = false;
		m_shiftKey = shift && !m_shiftSpent && (m_shiftKey || focused || isMouseOver(true));
		m_shift->setKeyHeld(m_shiftKey);
		for(size_t i = 0; i < 4; ++i)
			m_slotButtons[i]->setKeyHeld(focused && juce::KeyPress::isKeyCurrentlyDown(g_slotKeys[i]));
	}

	void Panel::timerCallback()
	{
		m_lcd.repaint();
		updateHeldKeys();
		std::array<g1::KnobInfo, 18> info{};
		for(uint32_t k = 0; k < 18; ++k)
			info[k] = m_knobMap.read(k);
		updateKnobs(info);
		updateLcdTips(info);
		for(auto& [led, bit] : m_ledMap)
			led->setOn(ledLit(bit));
		updateStatus();
	}

	// The knobs follow the G1's own positions, which something else may have moved (the plugin's
	// host automation), except the one being turned by hand. Following the patch, a knob shows
	// instead where its parameter's value would put it, as a patch loaded on the hardware leaves
	// its knobs where they were; turning it starts from there. A knob with nothing assigned, or on
	// a morph group (left to its own position for now), shows its position.
	void Panel::updateKnobs(const std::array<g1::KnobInfo, 18>& _info)
	{
		const bool follow = m_followToggle.getToggleState();
		const auto now = juce::Time::getMillisecondCounter();
		for(size_t i = 0; i < m_knobs.size(); ++i)
		{
			if(m_knobs[i].isMouseButtonDown())
				continue;
			const auto& k = _info[i];
			const bool settling = m_knobTurnedAt[i] != 0 && now - m_knobTurnedAt[i] < g_knobSettleMs;
			const bool fromPatch = follow && k.assigned && k.section != 2 && !settling;
			m_knobs[i].setValue(fromPatch ? g1::KnobMap::positionFor(k.value, k.max) : m_mc.adc(g_knobAdc[i]), juce::dontSendNotification);
		}
		if(!m_volume.isMouseButtonDown())
			m_volume.setValue(m_mc.adc(VolumeAdc), juce::dontSendNotification);
		if(m_knobDisplays[0].isVisible())
			for(uint32_t k = 0; k < 18; ++k)
				m_knobDisplays[k].set(_info[k]);
	}

	namespace
	{
		// What a knob moves, for the tooltips' display, as its display says it: with the displays
		// off, hovering a knob still tells. The value after a tab, at the right. Its number is on
		// the panel already.
		juce::String knobTip(const g1::KnobInfo& _k)
		{
			if(!_k.assigned)
				return "Nothing assigned";
			const auto name = juce::String(_k.moduleName) + ", " + juce::String(_k.paramName);
			return name + "\t" + juce::String(g1::formatValue(_k.type, _k.param, _k.value));
		}
	}

	juce::String Panel::shiftedKnobTip(const g1::KnobInfo& _k) const
	{
		if(m_mode == Mode::Edit)
			return _k.assigned ? "Clear Knob" : knobTip(_k);
		if(m_mode == Mode::Patch || m_mode == Mode::System)
			return {};
		return knobTip(_k);
	}

	// What the tooltips' display says for the controls whose text changes: the knobs, the master
	// volume (in the OS's steps: it takes the knob's position halved, NOTES.md) and the buttons
	// with a second function.
	void Panel::updateLcdTips(const std::array<g1::KnobInfo, 18>& _info)
	{
		updateMode();
		const bool shift = m_shift->isPressed();
		// With Shift down, a turn changes nothing (the OS ignores it), except in Edit mode, where it
		// takes an assigned knob's assignment away (the manual, Assign/Morph).
		for(size_t i = 0; i < m_knobs.size(); ++i)
			setLcdTip(m_knobs[i], shift ? shiftedKnobTip(_info[i]) : knobTip(_info[i]));
		setLcdTip(m_volume, "Master Volume\t" + juce::String(m_mc.adc(VolumeAdc) / 2));
		for(const auto& s : m_shiftNames)
			setLcdTip(*s.button, s.name(shift));
		updateTip();
	}

	void Panel::updateStatus()
	{
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

	// ________________________________________________________________________

	PanelView::PanelView(PanelHost& _host) : m_host(_host), m_panel(_host)
	{
		addAndMakeVisible(m_panel);
		m_panelHeight = m_panel.getHeight();
		const float s = std::clamp(m_host.panelScale(), MinScale, MaxScale);
		setSize(juce::roundToInt(static_cast<float>(m_panel.getWidth()) * s), juce::roundToInt(static_cast<float>(m_panel.getHeight()) * s));
	}

	void PanelView::applyLimits(juce::ComponentBoundsConstrainer& _c, const double _aspect)
	{
		const auto w = [](const float _s) { return juce::roundToInt(static_cast<float>(Panel::Width) * _s); };
		_c.setFixedAspectRatio(_aspect);
		_c.setSizeLimits(w(MinScale), juce::roundToInt(w(MinScale) / _aspect), w(MaxScale), juce::roundToInt(w(MaxScale) / _aspect));
	}

	// The panel as large as it fits, centred: a window whose resizer counts its title bar, or a
	// host that ignores the proportions, leaves a margin rather than cutting the panel.
	void PanelView::resized()
	{
		const float s = std::min(scale(), static_cast<float>(getHeight()) / static_cast<float>(m_panel.getHeight()));
		if(s <= 0)
			return;
		const float x = (static_cast<float>(getWidth()) - static_cast<float>(m_panel.getWidth()) * s) * 0.5f;
		const float y = (static_cast<float>(getHeight()) - static_cast<float>(m_panel.getHeight()) * s) * 0.5f;
		m_panel.setTransform(juce::AffineTransform::scale(s).translated(x, y));
		m_host.setPanelScale(s);
	}

	void PanelView::paint(juce::Graphics& _g)
	{
		_g.fillAll(g_face);
	}

	// The panel's own size changed (not its scale): the view follows, at the scale its width says.
	// The resizer gets the new proportions first: the window it resizes would otherwise be put back
	// to the old ones, and the panel shown smaller in it, with margins.
	void PanelView::childBoundsChanged(juce::Component* _child)
	{
		if(_child != &m_panel || getWidth() == 0 || m_panel.getHeight() == m_panelHeight)
			return;
		const float s = scale();
		m_panelHeight = m_panel.getHeight();
		if(onAspectChanged)
			onAspectChanged();
		setSize(getWidth(), juce::roundToInt(static_cast<float>(m_panel.getHeight()) * s));
	}
}
