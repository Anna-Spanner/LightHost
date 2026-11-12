cmake_minimum_required(VERSION 3.22)
project(LightHost VERSION 1.2.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Override with -DJUCE_DIR=<path to JUCE> if JUCE lives elsewhere
if(NOT DEFINED JUCE_DIR)
    foreach(candidate "${CMAKE_SOURCE_DIR}/LightHost/lib/juice" "${CMAKE_SOURCE_DIR}/lib/juce" "${CMAKE_SOURCE_DIR}/lib/juice")
        if(EXISTS "${candidate}/CMakeLists.txt")
            set(JUCE_DIR "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT JUCE_DIR)
    message(FATAL_ERROR "JUCE not found. Pass -DJUCE_DIR=<path to JUCE>")
endif()

add_subdirectory("${JUCE_DIR}" juce EXCLUDE_FROM_ALL)

set(VST2_SDK_DIR "${CMAKE_SOURCE_DIR}/VST2Compat" CACHE PATH
    "Directory containing JUCE-compatible VST2 interface headers")
if(NOT VST2_SDK_DIR)
    set(VST2_SDK_DIR "${CMAKE_SOURCE_DIR}/VST2Compat")
endif()
if(NOT EXISTS "${VST2_SDK_DIR}/pluginterfaces/vst2.x/aeffect.h"
   OR NOT EXISTS "${VST2_SDK_DIR}/pluginterfaces/vst2.x/aeffectx.h")
    message(FATAL_ERROR "VST2 support is required: VST2_SDK_DIR must contain pluginterfaces/vst2.x/aeffect.h and aeffectx.h")
endif()

juce_add_gui_app(LightHost
    PRODUCT_NAME "Light Host"
    COMPANY_NAME "Rolando Islas"
    BUNDLE_ID com.rolandoislas.lighthost
    ICON_BIG "${CMAKE_SOURCE_DIR}/Resources/icon.png")

juce_add_binary_data(LightHostData SOURCES
    Resources/icon.png
    Resources/menu_icon.png
    Resources/menu_icon_white.png)

juce_generate_juce_header(LightHost)

target_sources(LightHost PRIVATE
    Source/ErrorReporting.hpp
    Source/HostStartup.cpp
    Source/IconMenu.cpp
    Source/PluginChannelRouting.hpp
    Source/StereoRouting.hpp
    Source/UpdateManifestValidation.hpp
    Source/PluginWindow.cpp)

target_compile_definitions(LightHost PRIVATE
    JUCE_PLUGINHOST_VST=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_CATCH_UNHANDLED_EXCEPTIONS=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

if(WIN32)
    target_compile_definitions(LightHost PRIVATE JUCE_ASIO=1)
endif()

option(LIGHTHOST_BUILD_TESTS "Build Light Host regression tests" OFF)
if(LIGHTHOST_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(LightHostRoutingTest PRODUCT_NAME "Light Host Routing Test")
    juce_generate_juce_header(LightHostRoutingTest)
    target_sources(LightHostRoutingTest PRIVATE Tests/StereoRoutingTest.cpp)
    target_compile_definitions(LightHostRoutingTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(LightHostRoutingTest PRIVATE juce::juce_audio_utils)
    add_test(NAME LightHostRoutingTest COMMAND LightHostRoutingTest)
endif()
if(APPLE)
    target_compile_definitions(LightHost PRIVATE JUCE_PLUGINHOST_AU=1)
elseif(UNIX)
    target_compile_definitions(LightHost PRIVATE JUCE_PLUGINHOST_LV2=1)
endif()

target_include_directories(LightHost PRIVATE "${VST2_SDK_DIR}")

target_link_libraries(LightHost PRIVATE
    LightHostData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_gui_extra
    juce::juce_cryptography
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if(WIN32)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installer-$<CONFIG>.rc"
        COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:LightHost>"
                "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/installer-$<CONFIG>.rc"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Installer/WriteResource.cmake"
        DEPENDS LightHost "${CMAKE_CURRENT_SOURCE_DIR}/Installer/WriteResource.cmake"
        VERBATIM)
    add_executable(LightHostSetup WIN32 Installer/WindowsInstaller.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/installer-$<CONFIG>.rc")
    add_dependencies(LightHostSetup LightHost)
    target_link_libraries(LightHostSetup PRIVATE shell32 ole32 uuid)
    set_target_properties(LightHostSetup PROPERTIES OUTPUT_NAME "Light Host Setup")
endif()
