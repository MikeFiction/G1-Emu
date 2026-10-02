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

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <utility>
#include <memory>
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
		virtual juce::String settingsTooltip() const = 0;
		virtual void showSettings(juce::Component* _parent) = 0;
	};

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
		void setOn(bool _on) { if(_on != m_on) { m_on = _on; repaint(); } }
		void paint(juce::Graphics& _g) override;
	private:
		bool m_on = false;
	};

	// A panel button: while pressed, its matrix bit is 1. Without a known bit it is drawn
	// greyed out and does nothing. A mouse presses one button at a time, so a button can also be
	// held without it: from the computer's keyboard (Shift, A-D), or latched with a right click
	// (another right click lets it go). A held button is drawn down and lit.
	class PanelButton : public juce::Button
	{
	public:
		PanelButton(const juce::String& _name, g1::Microcontroller& _mc, MatrixBit _bit);
		void paintButton(juce::Graphics& _g, bool _over, bool _down) override;
		void setKeyHeld(bool _held) { if(_held != m_keyHeld) { m_keyHeld = _held; update(); } }
		void mouseDown(const juce::MouseEvent& _e) override;
		void mouseDrag(const juce::MouseEvent& _e) override;
		void mouseUp(const juce::MouseEvent& _e) override;
	private:
		void update();
		bool held() const { return m_keyHeld || m_latched; }
		g1::Microcontroller& m_mc;
		MatrixBit m_bit;
		bool m_down = false;
		bool m_keyHeld = false, m_latched = false;
	};

	// The dial: the rotary encoder to the right of the display. Dragging it up and down or
	// using the wheel turns it, and the emulator hands the OS its quadrature edges.
	class DialView : public juce::Component, public juce::SettableTooltipClient
	{
	public:
		explicit DialView(g1::Microcontroller& _mc) : m_mc(_mc) { setTooltip("Dial"); }
		void paint(juce::Graphics& _g) override;
		void mouseDown(const juce::MouseEvent& _e) override { m_lastY = _e.y; }
		void mouseDrag(const juce::MouseEvent& _e) override;
		void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& _w) override;
	private:
		void turn(int _detents);
		g1::Microcontroller& m_mc;
		int m_lastY = 0;
		float m_angle = 0;
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

	class KnobLook : public juce::LookAndFeel_V4
	{
	public:
		void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
	};

	class Panel : public juce::Component, private juce::Timer
	{
	public:
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
		PanelButton& addButton(const juce::String& _name, MatrixBit _bit);
		LedView& addLed(MatrixBit _bit);
		void reportIssue();
		void setExtrasOpen(bool _open);
		void randomizeKnobs();
		void restoreKnobs();
		void setKnobDisplays(bool _on);

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
		DoubleClickButton m_random{"Random"};
		juce::ToggleButton m_displaysToggle{"Parameter displays"};
		juce::Random m_rng;

		// What each knob is assigned to, from the OS's tables (g1knobs.h), and the displays.
		g1::KnobMap m_knobMap;
		std::array<KnobDisplay, 18> m_knobDisplays;
		// The patch's values, taken before the first Random, so a double click can put them
		// back. Dropped when the knobs' assignments change (another patch).
		std::array<g1::KnobInfo, 18> m_snapshot{};
		bool m_haveSnapshot = false;
		juce::TooltipWindow m_tooltips{this, 500};
		double m_peakHold = 0;
		uint64_t m_lastMidiIn = 0;
		int m_midiHold = 0;
	};
}
