cmake_minimum_required(VERSION 3.22)
project(VOICESEX VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/JUCE)
set(ORT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime-win-x64-1.23.2")
juce_add_binary_data(SexyModels SOURCES models/df_enc.onnx models/df_dec.onnx models/erb_enc.onnx models/erb_dec.onnx logo_ndertal.png)
juce_add_plugin(VOICESEX
    COMPANY_NAME "NDERTAL LABS"
    BUNDLE_ID "com.ndertallabs.voicesex"
    PLUGIN_MANUFACTURER_CODE Ndtl
    PLUGIN_CODE Vsx1
    FORMATS VST3
    PRODUCT_NAME "VOICESEX"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE VST3_AUTO_MANIFEST FALSE)
target_sources(VOICESEX PRIVATE Plugin.cpp Streaming.cpp Combinador.cpp)
target_compile_options(VOICESEX PRIVATE /utf-8)
target_include_directories(VOICESEX PRIVATE "${ORT}/include")
target_compile_definitions(VOICESEX PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VOICESEX PRIVATE SexyModels "${ORT}/lib/onnxruntime.lib" juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags delayimp)
target_link_options(VOICESEX_VST3 PRIVATE "/DELAYLOAD:onnxruntime.dll")
add_custom_command(TARGET VOICESEX_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORT}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:VOICESEX_VST3>/onnxruntime.dll")
juce_enable_vst3_manifest_step(VOICESEX)
juce_add_console_app(SexyStreamTest PRODUCT_NAME "SexyStreamTest")
target_sources(SexyStreamTest PRIVATE Streaming.cpp StreamTest.cpp Combinador.cpp)
target_include_directories(SexyStreamTest PRIVATE "${ORT}/include")
target_compile_definitions(SexyStreamTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SexyStreamTest PRIVATE SexyModels "${ORT}/lib/onnxruntime.lib" juce::juce_dsp juce::juce_audio_formats delayimp)
target_link_options(SexyStreamTest PRIVATE "/DELAYLOAD:onnxruntime.dll")
add_custom_command(TARGET SexyStreamTest POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORT}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:SexyStreamTest>/onnxruntime.dll")
juce_add_console_app(CombinadorTest PRODUCT_NAME "CombinadorTest")
target_sources(CombinadorTest PRIVATE Combinador.cpp CombinadorTest.cpp)
target_compile_definitions(CombinadorTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(CombinadorTest PRIVATE juce::juce_dsp)
juce_add_console_app(SexyProcessorTest PRODUCT_NAME "SexyProcessorTest")
target_sources(SexyProcessorTest PRIVATE ProcessorTest.cpp)
target_link_libraries(SexyProcessorTest PRIVATE VOICESEX)
target_include_directories(SexyProcessorTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules")
target_compile_definitions(SexyProcessorTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_options(SexyProcessorTest PRIVATE "/DELAYLOAD:onnxruntime.dll")
add_custom_command(TARGET SexyProcessorTest POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORT}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:SexyProcessorTest>/onnxruntime.dll")
