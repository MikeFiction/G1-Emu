# g1gui: the window with the G1 panel.
juce_add_gui_app(g1gui PRODUCT_NAME "G1-Emu")
target_sources(g1gui PRIVATE Main.cpp Panel.cpp Panel.h Settings.cpp Settings.h LcdFont.h)
# The build's version for the Report issue button: the tag and commit when Git knows them.
execute_process(COMMAND git describe --tags --always --dirty
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE g1_build_version
	OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT g1_build_version)
	set(g1_build_version "unknown")
endif()
target_compile_definitions(g1gui PRIVATE
	G1_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
	G1_BUILD_VERSION="${g1_build_version}"
	JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="G1-Emu" JUCE_APPLICATION_VERSION_STRING="0.1")
target_link_libraries(g1gui PRIVATE g1Host juce::juce_gui_basics juce::juce_data_structures juce::juce_recommended_config_flags)
if(G1_HAVE_WINDOWS_MIDI)
	g1_copy_midi_runtime(g1gui)
endif()
