cmake_minimum_required(VERSION 3.16...4.2)

project(OpenSoundMixer LANGUAGES C CXX)

option(BUILD_TEST "Build test" OFF)
option(DOWNLOAD_TEST_DATA "Download runtime assets for OpenSoundMixerTest" OFF)
option(SANITIZE_ENABLED "Enable address/undefined sanitizers in supported toolchains" OFF)

set(FONLINE_OGG_SOURCE_DIR "" CACHE PATH
    "Path to FOnline's engine-owned Ogg sources")
set(FONLINE_VORBIS_SOURCE_DIR "" CACHE PATH
    "Path to FOnline's engine-owned Vorbis sources")

if(NOT IS_DIRECTORY "${FONLINE_OGG_SOURCE_DIR}")
    message(FATAL_ERROR "Missing shared Ogg source: ${FONLINE_OGG_SOURCE_DIR}")
endif()
if(NOT IS_DIRECTORY "${FONLINE_VORBIS_SOURCE_DIR}")
    message(FATAL_ERROR "Missing shared Vorbis source: ${FONLINE_VORBIS_SOURCE_DIR}")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(BUILD_TEST)
    find_package(Python3 3 REQUIRED COMPONENTS Interpreter)

    set(test_data_stamp "${CMAKE_CURRENT_BINARY_DIR}/TestData.stamp")
    add_custom_command(
        OUTPUT "${test_data_stamp}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup.py" "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${test_data_stamp}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup.py"
        VERBATIM
    )
    add_custom_target(TestData DEPENDS "${test_data_stamp}")
endif()

file(GLOB_RECURSE osm_src CONFIGURE_DEPENDS
    src/Decorder/*.h
    src/Decorder/*.cpp
    src/Filter/*.h
    src/Filter/*.cpp)

set(osm_codec_src
    "${FONLINE_OGG_SOURCE_DIR}/src/bitwise.c"
    "${FONLINE_OGG_SOURCE_DIR}/src/framing.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/analysis.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/bitrate.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/block.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/codebook.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/envelope.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/floor0.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/floor1.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/info.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/lookup.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/lpc.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/lsp.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/mapping0.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/mdct.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/psy.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/registry.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/res0.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/sharedbook.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/smallft.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/synthesis.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/vorbisenc.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/vorbisfile.c"
    "${FONLINE_VORBIS_SOURCE_DIR}/lib/window.c")

list(APPEND osm_src ${osm_codec_src})

file(GLOB osm_root_src CONFIGURE_DEPENDS
    src/*.h
    src/*.cpp)

list(APPEND osm_src ${osm_root_src})

set(wasapi_src
    src/Manager/osm.Manager_Impl_WasApi.cpp)

set(pulse_audio_src
    src/Manager/osm.Manager_Impl_PulseAudio.cpp)

set(open_al_src
    src/Manager/osm.Manager_Impl_OpenAL.cpp)

if(BUILD_TEST)
    set(test_src
        test/main.cpp)
endif()

if(WIN32)
    list(APPEND osm_src ${wasapi_src})
elseif(APPLE)
    list(APPEND osm_src ${open_al_src})
else()
    list(APPEND osm_src ${pulse_audio_src})
endif()

set(files ${osm_src} ${test_src})

foreach(FILE ${files})
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    string(REPLACE "/" "\\" GROUP "${GROUP}")
    source_group("${GROUP}" FILES "${FILE}")
endforeach()

add_library(OpenSoundMixer STATIC ${osm_src})
target_include_directories(OpenSoundMixer
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE
        "${FONLINE_OGG_SOURCE_DIR}/include"
        "${FONLINE_VORBIS_SOURCE_DIR}/include"
        "${FONLINE_VORBIS_SOURCE_DIR}/lib")

if(BUILD_TEST)
    add_executable(OpenSoundMixerTest ${test_src})
    target_link_libraries(OpenSoundMixerTest PRIVATE OpenSoundMixer)
    if(DOWNLOAD_TEST_DATA)
        add_dependencies(OpenSoundMixerTest TestData)
    endif()
endif()

if(SANITIZE_ENABLED AND NOT MSVC)
    target_compile_options(OpenSoundMixer PRIVATE -fsanitize=address,undefined)
    target_link_options(OpenSoundMixer PRIVATE -fsanitize=address,undefined)

    if(BUILD_TEST)
        target_compile_options(OpenSoundMixerTest PRIVATE -fsanitize=address,undefined)
        target_link_options(OpenSoundMixerTest PRIVATE -fsanitize=address,undefined)
    endif()
endif()

if(NOT MSVC)
    find_package(Threads REQUIRED)
    target_link_libraries(OpenSoundMixer PUBLIC Threads::Threads)
endif()

if(APPLE)
    find_package(OpenAL REQUIRED)
    if(TARGET OpenAL::OpenAL)
        target_link_libraries(OpenSoundMixer PUBLIC OpenAL::OpenAL)
    else()
        target_include_directories(OpenSoundMixer PRIVATE "${OPENAL_INCLUDE_DIR}")
        target_link_libraries(OpenSoundMixer PUBLIC "${OPENAL_LIBRARY}")
    endif()
elseif(UNIX AND NOT WIN32)
    set_property(TARGET OpenSoundMixer PROPERTY POSITION_INDEPENDENT_CODE TRUE)

    find_package(PulseAudio REQUIRED)
    if(TARGET PulseAudio::PulseAudio)
        target_link_libraries(OpenSoundMixer PUBLIC PulseAudio::PulseAudio)
    else()
        target_include_directories(OpenSoundMixer PRIVATE "${PULSEAUDIO_INCLUDE_DIR}")
        target_link_libraries(OpenSoundMixer PUBLIC "${PULSEAUDIO_LIBRARY}")
    endif()

    target_link_libraries(OpenSoundMixer PUBLIC dl)
endif()

if(BUILD_TEST)
    set_target_properties(OpenSoundMixerTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

set_target_properties(OpenSoundMixer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

install(
    TARGETS OpenSoundMixer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)

if(BUILD_TEST)
    install(
        TARGETS OpenSoundMixerTest
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION bin
        LIBRARY DESTINATION bin)
endif()
