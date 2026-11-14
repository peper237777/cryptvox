cmake_minimum_required(VERSION 3.22)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
project(Cryptvox VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Cryptvox
  COMPANY_NAME "RapTools"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  PLUGIN_MANUFACTURER_CODE Rapt
  PLUGIN_CODE Cvx1
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  PRODUCT_NAME "Cryptvox")

target_sources(Cryptvox PRIVATE
  PluginProcessor.cpp PluginProcessor.h
  PluginEditor.cpp PluginEditor.h Params.h)

target_compile_definitions(Cryptvox PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Cryptvox
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags)
