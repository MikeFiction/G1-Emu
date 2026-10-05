#pragma once

// The synth settings over the panel (extras: "Synth Settings"): the slots' MIDI channels and the
// settings that apply to all of them, as the System menu and NME's Synth Settings dialog have
// them. Each change goes to the OS at once (SynthSettingsLink), which then reports what it took.
// What is behind the card shows through it blurred, as taken when the overlay opened.

#include "synthsettings.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace g1gui
{
	// The boxes' text a little smaller than JUCE's, to sit with the labels.
	class SettingsLook : public juce::LookAndFeel_V4
	{
	public:
		juce::Font getComboBoxFont(juce::ComboBox&) override;
		juce::Font getPopupMenuFont() override;
	};

	class SynthSettingsView : public juce::Component, private juce::Timer
	{
	public:
		explicit SynthSettingsView(g1app::SynthSettingsLink& _link);
		~SynthSettingsView() override;

		// Shows it over _behind (its parent), which is photographed for the backdrop first. Its top
		// _faceHeight (the synth itself) is darkened; what is below, its status bar and extras, is not.
		// The card's top sits on _space's (the top of the synth's purple sections), centred across it.
		void open(juce::Component& _behind, int _faceHeight, juce::Rectangle<int> _space);
		void close();
		std::function<void()> onClose;

		void paint(juce::Graphics& _g) override;
		void resized() override;
		void mouseDown(const juce::MouseEvent& _e) override;
		bool keyPressed(const juce::KeyPress& _key) override;

	private:
		void timerCallback() override;
		void show(const g1app::SynthSettings& _s);	// into the controls
		void apply();								// the controls to the OS
		juce::Rectangle<int> card() const;

		g1app::SynthSettingsLink& m_link;
		SettingsLook m_look;
		g1app::SynthSettings m_known;	// what the OS said last: fields with no control keep theirs
		uint64_t m_revision = 0;
		bool m_haveSettings = false;
		juce::Image m_backdrop;
		int m_faceHeight = 0;
		juce::Rectangle<int> m_space;

		juce::Label m_nameLabel;
		juce::TextEditor m_name;
		std::array<juce::Label, 4> m_slotLabels;
		std::array<juce::ComboBox, 4> m_channels;
		juce::Label m_keyboardLabel, m_clockLabel, m_bpmLabel, m_syncLabel, m_tuneLabel;
		juce::ComboBox m_keyboard, m_clock, m_bpm, m_sync, m_tune;
		juce::Label m_knobLabel, m_pedalLabel, m_programLabel, m_localLabel, m_ledsLabel, m_velMinLabel, m_velMaxLabel;
		juce::ComboBox m_knob, m_pedal, m_program, m_local, m_leds, m_velMin, m_velMax;
		juce::Label m_note;
		juce::TextButton m_close{"Close"};
	};
}
