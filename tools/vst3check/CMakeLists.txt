# g1vst3check: the built VST3 loaded through JUCE's VST3 host, as a DAW would (see the source).
juce_add_console_app(g1vst3check PRODUCT_NAME "g1vst3check")
target_sources(g1vst3check PRIVATE vst3check.cpp)
target_compile_definitions(g1vst3check PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
	JUCE_MODAL_LOOPS_PERMITTED=1)
# The VST3 SDK's interfaces, to call the plugin's IComponent as Cubase does (--buffer-change, #66).
target_include_directories(g1vst3check PRIVATE ${G1_JUCE_DIR}/modules/juce_audio_processors_headless/format_types/VST3_SDK)
target_link_libraries(g1vst3check PRIVATE juce::juce_audio_processors juce::juce_audio_devices juce::juce_gui_basics juce::juce_recommended_config_flags)
