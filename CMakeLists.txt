cmake_minimum_required(VERSION 3.22)
project(SEXYVOX VERSION 0.1.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(JUCE_DIR "" CACHE PATH "Path to an external JUCE 8.0.12 source checkout")
set(ONNXRUNTIME_DIR "" CACHE PATH "Path to an external ONNX Runtime 1.23.2 x64 SDK")
set(SEXYVOX_MODELS_DIR "" CACHE PATH "Path to four compatible, licensed ONNX graphs")
if(NOT WIN32)
    message(FATAL_ERROR "This version targets Windows x64")
endif()
foreach(graph df_enc df_dec erb_enc erb_dec)
    if(NOT EXISTS "${SEXYVOX_MODELS_DIR}/${graph}.onnx")
        message(FATAL_ERROR "Provide compatible models using SEXYVOX_MODELS_DIR; model weights are not distributed")
    endif()
endforeach()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
juce_add_binary_data(SexyModels SOURCES
    "${SEXYVOX_MODELS_DIR}/df_enc.onnx" "${SEXYVOX_MODELS_DIR}/df_dec.onnx"
    "${SEXYVOX_MODELS_DIR}/erb_enc.onnx" "${SEXYVOX_MODELS_DIR}/erb_dec.onnx")
juce_add_plugin(SEXYVOX COMPANY_NAME "VOICESEX audio" BUNDLE_ID "audio.voicesex.sexyvox"
    PLUGIN_MANUFACTURER_CODE Vsxa PLUGIN_CODE Sxv1 FORMATS VST3 PRODUCT_NAME "SEXYVOX"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE VST3_AUTO_MANIFEST FALSE)
target_sources(SEXYVOX PRIVATE src/Plugin.cpp src/Streaming.cpp)
target_include_directories(SEXYVOX PRIVATE "${ONNXRUNTIME_DIR}/include")
target_compile_options(SEXYVOX PRIVATE /utf-8)
target_compile_definitions(SEXYVOX PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SEXYVOX PRIVATE SexyModels "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
    juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags delayimp)
target_link_options(SEXYVOX_VST3 PRIVATE "/DELAYLOAD:onnxruntime.dll")
add_custom_command(TARGET SEXYVOX_VST3 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:SEXYVOX_VST3>/onnxruntime.dll")
juce_enable_vst3_manifest_step(SEXYVOX)

option(SEXYVOX_BUILD_TESTS "Build offline parity and paced realtime test tools" OFF)
if(SEXYVOX_BUILD_TESTS)
    juce_add_console_app(SexyProcessorTest PRODUCT_NAME "SexyProcessorTest")
    target_sources(SexyProcessorTest PRIVATE tests/ProcessorTest.cpp)
    target_include_directories(SexyProcessorTest PRIVATE "${JUCE_DIR}/modules")
    target_compile_definitions(SexyProcessorTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
    target_link_libraries(SexyProcessorTest PRIVATE SEXYVOX)
    target_link_options(SexyProcessorTest PRIVATE "/DELAYLOAD:onnxruntime.dll")
    add_custom_command(TARGET SexyProcessorTest POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:SexyProcessorTest>/onnxruntime.dll")
    juce_add_console_app(SexyStreamTest PRODUCT_NAME "SexyStreamTest")
    target_sources(SexyStreamTest PRIVATE src/Streaming.cpp tests/StreamTest.cpp)
    target_include_directories(SexyStreamTest PRIVATE src "${ONNXRUNTIME_DIR}/include")
    target_compile_definitions(SexyStreamTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SexyStreamTest PRIVATE SexyModels "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
        juce::juce_dsp juce::juce_audio_formats delayimp)
    target_link_options(SexyStreamTest PRIVATE "/DELAYLOAD:onnxruntime.dll")
    add_custom_command(TARGET SexyStreamTest POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:SexyStreamTest>/onnxruntime.dll")
endif()
