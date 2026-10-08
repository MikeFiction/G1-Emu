# The G1 as a VST3 instrument (docs/instance-hosting.md, issue #7). It is built on g1Core only:
# no ALSA, no JACK, no MIDI ports of its own. The window is g1gui's panel.
juce_add_plugin(g1plugin
	COMPANY_NAME "Animatek"
	PLUGIN_MANUFACTURER_CODE Atek
	PLUGIN_CODE G1Em
	PRODUCT_NAME "G1-Emu"
	IS_SYNTH TRUE
	NEEDS_MIDI_INPUT TRUE
	NEEDS_MIDI_OUTPUT FALSE
	IS_MIDI_EFFECT FALSE
	VST3_CATEGORIES Instrument Synth
	COPY_PLUGIN_AFTER_BUILD FALSE
	VST3_AUTO_MANIFEST FALSE
	FORMATS VST3)

target_sources(g1plugin PRIVATE Processor.cpp Processor.h Editor.cpp Editor.h runner.cpp runner.h
	../gui/Panel.cpp ../gui/Panel.h ../gui/SynthSettingsView.cpp ../gui/SynthSettingsView.h ../gui/Overlay.cpp ../gui/Overlay.h ../gui/LcdFont.h)
target_include_directories(g1plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ../gui)
target_compile_definitions(g1plugin PUBLIC
	G1_BUILD_VERSION="${g1_build_version}"
	JUCE_WEB_BROWSER=0
	JUCE_USE_CURL=0
	JUCE_VST3_CAN_REPLACE_VST2=0
	JUCE_DISPLAY_SPLASH_SCREEN=0)
target_sources(g1plugin PRIVATE ${G1_NME_SOURCES})
target_include_directories(g1plugin PRIVATE ${G1_NME_INCLUDE})
target_link_libraries(g1plugin PRIVATE g1Core g1Skin g1NmeData juce::juce_audio_utils
	PUBLIC juce::juce_recommended_config_flags)

# The same plugin as CLAP (G1-Emu.clap), when the Gearmulator clone has clap-juce-extensions.
if(G1_HAVE_CLAP)
	clap_juce_extensions_plugin(TARGET g1plugin
		CLAP_ID "net.animatek.g1-emu"
		CLAP_FEATURES instrument synthesizer
		CLAP_SUPPORT_URL "https://github.com/animatek/G1-Emu/issues"
		CLAP_MANUAL_URL "https://github.com/animatek/G1-Emu")
endif()

# Knobs turned on the panel, told to the host as gestures (issue #42): no ROM, no DAW.
add_executable(g1hostgesturestest hostgesturestest.cpp hostgestures.h)
target_include_directories(g1hostgesturestest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME g1hostgesturestest COMMAND g1hostgesturestest)
set_tests_properties(g1hostgesturestest PROPERTIES TIMEOUT 60 LABELS g1)

# The host's transport as MIDI clock (issue #20): no ROM, no DAW.
add_executable(g1hostclocktest hostclocktest.cpp hostclock.h)
target_include_directories(g1hostclocktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME g1hostclocktest COMMAND g1hostclocktest)
set_tests_properties(g1hostclocktest PROPERTIES TIMEOUT 60 LABELS g1)

# The runner without a DAW: blocks of any size, notes at exact frames, offline and in real time.
# Needs a ROM, so without one it reports itself skipped.
add_executable(g1runnertest runnertest.cpp runner.cpp runner.h)
target_include_directories(g1runnertest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(g1runnertest PRIVATE g1Core)
add_test(NAME g1runnertest COMMAND g1runnertest)
set_tests_properties(g1runnertest PROPERTIES TIMEOUT 300 LABELS g1 SKIP_RETURN_CODE 77)

# The runner when the DSP JIT cannot generate a block (#17): one failure it recovers from, and a
# JIT that keeps failing, which must give silence and not hang or crash. Needs a ROM.
add_executable(g1jitfailtest jitfailtest.cpp runner.cpp runner.h)
target_include_directories(g1jitfailtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(g1jitfailtest PRIVATE g1Core)
add_test(NAME g1jitfailtest-recover COMMAND g1jitfailtest recover)
add_test(NAME g1jitfailtest-fail COMMAND g1jitfailtest fail)
set_tests_properties(g1jitfailtest-recover g1jitfailtest-fail PROPERTIES TIMEOUT 300 LABELS g1 SKIP_RETURN_CODE 77)
