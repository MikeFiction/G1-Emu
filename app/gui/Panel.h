#pragma once

// The panel of the emulated G1, like the hardware's: the display, the volume and the 18 knobs
// with their LEDs, the buttons, and a status bar with speed and load. It reads and writes the
// emulator's panel (Microcontroller: getLcd, ledRow, setButton, setAdc), which can be used from
// this thread. Where every button and LED sits in the matrices: see NOTES.md, "The panel".
//
// The same panel is the window of g1gui and the editor of the plugin: what it needs from
// whoever runs the G1 is PanelHost.

#include "hostconfig.h"
#include "g1Lib/g1knobs.h"
#include "SynthSettingsView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <utility>
#include <memory>
#include <optional>
#include <vector>

namespace g1gui
{
	// The notice about Clavia, ROMs and support. It is the same text as the README's "Please read
	// this first", shown in the settings window and, the first time G1-Emu runs, at startup; the plugin shows it
	// in its Settings.
	const char* disclaimerText();

	struct MatrixBit { int row = -1, bit = -1; bool known() const { return row >= 0; } };

	// What the panel needs from whoever runs the G1: the G1 itself, the figures for the status
	// bar, where to keep the panel's own preferences, and what its Settings button opens.
	class PanelHost
	{
	public:
		virtual ~PanelHost() = default;
		virtual g1::Microcontroller& mc() = 0;
		virtual g1app::HostStats stats() = 0;
		virtual bool extrasOpen() const = 0;
		virtual void setExtrasOpen(bool _open) = 0;
		virtual bool knobDisplays() const = 0;
		virtual void setKnobDisplays(bool _on) = 0;
		virtual bool knobFollowsPatch() const = 0;		// a knob shows its parameter's value, not where it was turned
		virtual void setKnobFollowsPatch(bool _on) = 0;
		virtual float panelScale() const = 0;			// the window's size, as PanelView keeps it
		virtual void setPanelScale(float _scale) = 0;
		virtual juce::String settingsTooltip() const = 0;
		virtual void showSettings(juce::Component* _parent) = 0;
		virtual g1app::SynthSettingsLink& synthSettings() = 0;	// the OS's, for the extras' overlay
	};

	// The panel is drawn from the PNGs in skin/ (built in as G1Skin), laid out in the background's
	// own pixels (3000 x 1238) and shown at SkinScale of them.
	struct Skin;

	// The display's glass: the red frame around it is the background's.
	class LcdView : public juce::Component
	{
	public:
		explicit LcdView(const g1::Lcd& _lcd) : m_lcd(_lcd) {}
		void paint(juce::Graphics& _g) override;
	private:
		const g1::Lcd& m_lcd;
	};

	class LedView : public juce::Component
	{
	public:
		LedView() { setInterceptsMouseClicks(false, false); }
		void setOn(bool _on) { if(_on != m_on) { m_on = _on; repaint(); } }
		void paint(juce::Graphics& _g) override;
	private:
		bool m_on = false;
	};

	// A panel button: while pressed, its matrix bit is 1. Without a known bit it is drawn
	// greyed out and does nothing. A mouse presses one button at a time, so a button can also be
	// held without it: from the computer's keyboard (Shift, A-D), or latched with a right click
	// (another right click lets it go). A held button is drawn down and lit. Its bounds are the
	// whole sprite, shadow included; only the button itself takes the mouse.
	class PanelButton : public juce::Button, private juce::Timer
	{
	public:
		enum class Shape { Wide, Tall, Tilted };	// the navigator's up/down are tall, Assign/Morph tilted
		PanelButton(const juce::String& _name, g1::Microcontroller& _mc, MatrixBit _bit, Shape _shape = Shape::Wide);
		void paintButton(juce::Graphics& _g, bool _over, bool _down) override;
		bool hitTest(int _x, int _y) override;
		void setKeyHeld(bool _held) { if(_held != m_keyHeld) { m_keyHeld = _held; update(); } }
		// Held with the mouse, it presses again and again after a moment, as a key that repeats:
		// the G1 takes a held key as one press (the navigator's). Such a key does not latch.
		void setAutoRepeat(bool _on);
		// A key whose being held does nothing (Panel Split toggles when pressed): no latching.
		void setLatchable(bool _on);
		void mouseDown(const juce::MouseEvent& _e) override;
		void mouseDrag(const juce::MouseEvent& _e) override;
		void mouseUp(const juce::MouseEvent& _e) override;
	private:
		void update();
		void timerCallback() override;
		void setBit(bool _down);
		bool held() const { return m_keyHeld || m_latched; }
		bool repeating() const { return m_autoRepeat && m_down && isDown(); }
		g1::Microcontroller& m_mc;
		MatrixBit m_bit;
		Shape m_shape;
		bool m_down = false;
		bool m_keyHeld = false, m_latched = false;
		bool m_autoRepeat = false, m_repeatUp = false;	// m_repeatUp: let go for a moment between presses
		bool m_wasHeld = false;
		bool m_latchable = true;
		juce::uint32 m_pressedAt = 0;
	};

	// The dial: the rotary encoder to the right of the display. Dragging it up and down or
	// using the wheel turns it, and the emulator hands the OS its quadrature edges.
	class DialView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
	{
	public:
		explicit DialView(g1::Microcontroller& _mc) : m_mc(_mc) { setTooltip("Data Wheel"); }
		void paint(juce::Graphics& _g) override;
		bool hitTest(int _x, int _y) override;
		void mouseDown(const juce::MouseEvent& _e) override { m_lastY = _e.y; }
		void mouseDrag(const juce::MouseEvent& _e) override;
		void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& _w) override;
	private:
		void turn(int _detents);
		void timerCallback() override;
		g1::Microcontroller& m_mc;
		int m_lastY = 0;
		// Degrees turned, clockwise from the indent straight up, shown and asked for: the picture
		// follows. It starts with the indent at about eight o'clock, where Mike Fiction likes it.
		static constexpr float StartAngle = 255.0f;
		float m_angle = StartAngle, m_targetAngle = StartAngle;
	};

	// A small square button with an icon instead of text, for what is not on the hardware
	// (Settings, Report issue, Patreon). The tooltip says what it does.
	class IconButton : public juce::Button
	{
	public:
		enum class Icon { Settings, Report, Patreon, ExtrasOpen, ExtrasClose };
		IconButton(const juce::String& _name, Icon _icon) : juce::Button(_name), m_icon(_icon) {}
		void setIcon(Icon _icon) { m_icon = _icon; repaint(); }
		void paintButton(juce::Graphics& _g, bool _over, bool _down) override;
	private:
		Icon m_icon;
	};

	// A small display above a knob, in the colours of the G1's own: the module and value on
	// top, the parameter below, as the OS names them. Blank when the knob has nothing.
	class KnobDisplay : public juce::Component
	{
	public:
		void set(const g1::KnobInfo& _info);
		void paint(juce::Graphics& _g) override;
	private:
		juce::String m_top, m_bottom;
		bool m_assigned = false;
	};

	// A button that also answers a double click (Random: back to the patch's values). The double
	// click arrives on the second press, and its release would still be a click: that one is eaten.
	class DoubleClickButton : public juce::TextButton
	{
	public:
		using juce::TextButton::TextButton;
		std::function<void()> onSingleClick, onDoubleClick;
		void mouseDoubleClick(const juce::MouseEvent&) override
		{
			m_eatClick = true;
			if(onDoubleClick)
				onDoubleClick();
		}
	protected:
		void clicked() override
		{
			if(std::exchange(m_eatClick, false))
				return;
			if(onSingleClick)
				onSingleClick();
		}
	private:
		bool m_eatClick = false;
	};

	// The extras drawer's tray, with what it holds as children. It slides inside a clip below the
	// status bar, so it comes out from under it instead of appearing.
	class ExtrasDrawer : public juce::Component
	{
	public:
		void paint(juce::Graphics& _g) override;
	};

	class KnobLook : public juce::LookAndFeel_V4
	{
	public:
		void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
	};

	// The panel's tooltips: on a dark glass that blurs what is behind it.
	// _window is the tooltip window it draws, a child of the panel: what is behind it is the panel.
	class TooltipLook : public juce::LookAndFeel_V4
	{
	public:
		explicit TooltipLook(juce::Component& _window) : m_window(_window) {}
		juce::Rectangle<int> getTooltipBounds(const juce::String& _tip, juce::Point<int> _pos, juce::Rectangle<int> _parentArea) override;
		void drawTooltip(juce::Graphics& _g, const juce::String& _tip, int _w, int _h) override;
	private:
		juce::Component& m_window;
		bool m_snapshotting = false;	// while the panel behind is drawn for the blur, the tooltip is not
	};

	class Panel : public juce::Component, private juce::Timer
	{
	public:
		static constexpr float SkinScale = 0.4f;	// of the background's pixels: 3000 x 1238 shown as 1200 x 495
		static constexpr int Width = 1200, FaceHeight = 495, Height = FaceHeight + 36;	// the status bar below the face
		static constexpr juce::uint32 FaceColour = 0xff35263d;	// the skin's dark purple, for what is around it
		static constexpr uint8_t VolumeAdc = 0x30;				// the master volume's knob

		explicit Panel(PanelHost& _host);
		~Panel() override;
		void paint(juce::Graphics& _g) override;
		void resized() override;
		bool keyPressed(const juce::KeyPress& _key) override;
		bool keyStateChanged(bool _isKeyDown) override;
		void mouseDown(const juce::MouseEvent& _e) override;
		void focusLost(FocusChangeType) override { updateHeldKeys(); }

	private:
		void timerCallback() override;
		void updateHeldKeys();
		PanelButton& addButton(const juce::String& _name, MatrixBit _bit, PanelButton::Shape _shape = PanelButton::Shape::Wide);
		LedView& addLed(MatrixBit _bit);
		void reportIssue();
		void setExtrasOpen(bool _open, bool _animate = false);
		void slideDrawer(double _now);
		void placeDrawer();
		void randomizeKnobs();
		void restoreKnobs();
		void setKnobDisplays(bool _on);

		juce::SharedResourcePointer<Skin> m_skin;	// keeps the images while a panel is open
		PanelHost& m_host;
		g1::Microcontroller& m_mc;
		KnobLook m_knobLook;

		LcdView m_lcd;
		juce::Slider m_volume;
		std::array<juce::Slider, 18> m_knobs;
		std::array<LedView*, 18> m_knobLeds{};
		std::vector<std::unique_ptr<PanelButton>> m_buttons;
		std::vector<std::unique_ptr<LedView>> m_leds;
		std::vector<std::pair<LedView*, MatrixBit>> m_ledMap;

		LedView* m_midiLed = nullptr;
		LedView* m_panelSplitLed = nullptr;
		PanelButton* m_panelSplit = nullptr;
		PanelButton* m_find = nullptr;
		std::array<PanelButton*, 4> m_modeButtons{};	// Store, System, Edit, Patch/Load
		std::array<LedView*, 4> m_modeLeds{};
		std::array<PanelButton*, 4> m_slotButtons{};	// A-D
		std::array<LedView*, 4> m_slotLeds{};
		PanelButton* m_assign = nullptr;
		PanelButton* m_shift = nullptr;
		bool m_shiftKey = false;	// Shift on the computer's keyboard, held
		std::array<PanelButton*, 4> m_nav{};			// up, left, right, down
		DialView m_dial;

		juce::Label m_status;
		IconButton m_settings{"Settings", IconButton::Icon::Settings};
		IconButton m_report{"Report issue", IconButton::Icon::Report};
		IconButton m_patreon{"Patreon", IconButton::Icon::Patreon};
		IconButton m_extras{"Extras", IconButton::Icon::ExtrasOpen};

		// The extras drawer below the panel: what the hardware never had. Closed, the window is
		// the panel alone.
		bool m_extrasOpen = false;
		juce::Component m_drawerClip;
		ExtrasDrawer m_drawer;
		float m_drawerShown = 0.0f;					// 0 closed, 1 open
		float m_slideFrom = 0.0f;
		double m_slideStart = -1.0;
		std::optional<juce::VBlankAttachment> m_slide;	// while it slides
		DoubleClickButton m_random{"Random"};
		juce::ToggleButton m_displaysToggle{"Parameter Displays"};
		juce::ToggleButton m_followToggle{"Knob Follows Patch"};
		juce::TextButton m_synthButton{"Synth Settings"};
		SynthSettingsView m_synthView;	// over everything while open
		juce::Random m_rng;

		// What each knob is assigned to, from the OS's tables (g1knobs.h), and the displays.
		g1::KnobMap m_knobMap;
		std::array<KnobDisplay, 18> m_knobDisplays;
		// The patch's values, taken before the first Random, so a double click can put them
		// back. Dropped when the knobs' assignments change (another patch).
		std::array<g1::KnobInfo, 18> m_snapshot{};
		bool m_haveSnapshot = false;
		juce::TooltipWindow m_tooltips{this, 500};
		TooltipLook m_tooltipLook{m_tooltips};
		double m_peakHold = 0;
		uint64_t m_lastMidiIn = 0;
		int m_midiHold = 0;
	};

	// The panel at any size: it is laid out at Panel::Width and shown scaled to this view's width,
	// which keeps the panel's proportions (the window's resizer has to: aspectRatio). The scale is
	// the host's to keep. When the panel changes its own height (the extras drawer), the view
	// follows at the same scale and calls onAspectChanged.
	class PanelView : public juce::Component
	{
	public:
		static constexpr float MinScale = 0.5f, MaxScale = 2.5f;

		explicit PanelView(PanelHost& _host);
		double aspectRatio() const { return static_cast<double>(m_panel.getWidth()) / static_cast<double>(m_panel.getHeight()); }
		// The resizer's limits for these proportions, from MinScale to MaxScale.
		void applyLimits(juce::ComponentBoundsConstrainer& _c) const;
		std::function<void()> onAspectChanged;

		void resized() override;
		void paint(juce::Graphics& _g) override;
		void childBoundsChanged(juce::Component* _child) override;

	private:
		float scale() const { return static_cast<float>(getWidth()) / static_cast<float>(m_panel.getWidth()); }
		PanelHost& m_host;
		Panel m_panel;
		int m_panelHeight = 0;	// the panel's own, to tell the drawer opening from a new scale
	};
}
