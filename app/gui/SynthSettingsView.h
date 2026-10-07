#pragma once

// The synth settings over the panel (its Synth Settings page button): the slots' MIDI channels
// and the settings that apply to all of them, as the System menu and NME's Synth Settings dialog
// have them. Each change goes to the OS at once (SynthSettingsLink), which then reports what it
// took. It sits in Mike Fiction's frame (skin/settings_panel.png), over the knobs' four sections.

#include "synthsettings.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <map>

namespace g1gui
{
	// One of the skin's pictures, or a sheet of square frames (_frameSize), scaled to the screen's
	// pixels where it is drawn: each frame once, kept until that size changes.
	class SkinImage
	{
	public:
		SkinImage(const void* _png, int _pngSize, int _frameSize = 0);
		void draw(juce::Graphics& _g, juce::Rectangle<float> _dest, int _frame = 0);
	private:
		juce::Image m_source;
		int m_frameSize;
		juce::Point<int> m_size;				// what m_frames are scaled to
		std::map<int, juce::Image> m_frames;
	};

	// The boxes and the name field as rounded boxes with white text, as Mike Fiction's
	// Nord Lead 2x settings have them.
	class SettingsLook : public juce::LookAndFeel_V4
	{
	public:
		SettingsLook();
		juce::Font getComboBoxFont(juce::ComboBox&) override;
		juce::Font getPopupMenuFont() override;
		void drawComboBox(juce::Graphics&, int _w, int _h, bool _down, int, int, int, int, juce::ComboBox&) override;
		void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
		void fillTextEditorBackground(juce::Graphics&, int _w, int _h, juce::TextEditor&) override;
		void drawTextEditorOutline(juce::Graphics&, int _w, int _h, juce::TextEditor&) override;
		// The panel's knob cap and its shadow.
		void drawRotarySlider(juce::Graphics&, int _x, int _y, int _w, int _h, float _pos, float, float, juce::Slider&) override;
		// Master tune's disk, ticks and name, round the knob centred at _centre.
		void drawTuneBackground(juce::Graphics& _g, juce::Point<float> _centre);

	private:
		SkinImage m_knob, m_shadow, m_tuneBg;
	};

	// A number in a box, for a range too long for a list (the clock's rate): dragged up or down,
	// turned with the wheel, or typed after a double click. Only digits go in, and what is typed is
	// brought into the range; Escape, or nothing typed, leaves the value as it was.
	class ValueBox : public juce::Component, public juce::SettableTooltipClient
	{
	public:
		ValueBox(int _min, int _max);
		void setValue(int _value);			// from outside: no onChange
		int getValue() const { return m_value; }
		std::function<void()> onChange;		// a change made here

		void paint(juce::Graphics& _g) override;
		void resized() override;
		void mouseDown(const juce::MouseEvent& _e) override;
		void mouseDrag(const juce::MouseEvent& _e) override;
		void mouseDoubleClick(const juce::MouseEvent&) override;
		void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& _wheel) override;

	private:
		void change(int _value);
		void commit();
		void cancel();

		const int m_min, m_max;
		int m_value, m_dragFrom = 0;
		juce::TextEditor m_editor;
	};

	class SynthSettingsView : public juce::Component, private juce::Timer
	{
	public:
		explicit SynthSettingsView(g1app::SynthSettingsLink& _link);
		~SynthSettingsView() override;

		// Shows it at _frame, in its parent's pixels (the knobs' sections); the rest of the panel
		// stays as it is and works.
		void open(juce::Rectangle<int> _frame);
		void close();
		std::function<void()> onClose;

		void paint(juce::Graphics& _g) override;
		void resized() override;
		bool keyPressed(const juce::KeyPress& _key) override;
		void mouseEnter(const juce::MouseEvent& _e) override;
		void mouseExit(const juce::MouseEvent& _e) override;

	private:
		void timerCallback() override;
		void repaintControl(const juce::MouseEvent& _e);	// the control under a mouse event, for its hover colour
		void show(const g1app::SynthSettings& _s);	// into the controls
		void apply();								// the controls to the OS
		void lcdTips();	// each control's name and value, for the panel's display

		g1app::SynthSettingsLink& m_link;
		SettingsLook m_look;
		g1app::SynthSettings m_known;	// what the OS said last: fields with no control keep theirs
		uint64_t m_revision = 0;
		int m_ticks = 0;				// the timer's, since it opened
		bool m_haveSettings = false;
		SkinImage m_frame;

		juce::Label m_nameLabel;
		juce::TextEditor m_name;
		std::array<juce::ComboBox, 4> m_channels;
		juce::ComboBox m_clock, m_sync;
		ValueBox m_bpm{24, 240};
		juce::Slider m_tune;			// a knob, its value on the panel's display (lcdTips)
		juce::ComboBox m_knob, m_pedal, m_program, m_leds;
		ValueBox m_velMin{0, 127}, m_velMax{0, 127};
		juce::Label m_note;
	};

	// The Presets page, in the same frame and place as the Synth Settings: the synth's banks and
	// programs, to be listed and loaded from here. Not done yet (the next release): for now it
	// says so.
	class PresetsView : public juce::Component
	{
	public:
		PresetsView();

		void open(juce::Rectangle<int> _frame);
		void close();
		std::function<void()> onClose;

		void paint(juce::Graphics& _g) override;
		bool keyPressed(const juce::KeyPress& _key) override;

	private:
		SkinImage m_frame;
	};
}
