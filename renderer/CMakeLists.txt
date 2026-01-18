cmake_minimum_required(VERSION 3.20)

project(renderer LANGUAGES CXX)

set(RENDERER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT DEFINED OPENFRAMEWORKS_DIR AND DEFINED ENV{OPENFRAMEWORKS_DIR})
    set(OPENFRAMEWORKS_DIR "$ENV{OPENFRAMEWORKS_DIR}")
endif()
if(APPLE AND NOT OPENFRAMEWORKS_DIR AND EXISTS "/Users/aweijnitz/openFrameworks/of_v0.12.1_osx_release")
    set(OPENFRAMEWORKS_DIR "/Users/aweijnitz/openFrameworks/of_v0.12.1_osx_release")
endif()
set(OPENFRAMEWORKS_DIR "${OPENFRAMEWORKS_DIR}" CACHE PATH "Path to openFrameworks root (with libs/openFrameworks)")

if(NOT OPENFRAMEWORKS_DIR)
    message(FATAL_ERROR "OPENFRAMEWORKS_DIR must be set to a valid openFrameworks installation (with libs/openFrameworks).")
endif()

set(OF_MAIN_HEADER "${OPENFRAMEWORKS_DIR}/libs/openFrameworks/ofMain.h")
if(NOT EXISTS "${OF_MAIN_HEADER}")
    message(FATAL_ERROR "Could not find ofMain.h at ${OF_MAIN_HEADER}. Set OPENFRAMEWORKS_DIR to your openFrameworks root.")
endif()

if(NOT OPENFRAMEWORKS_PLATFORM)
    if(APPLE)
        set(OPENFRAMEWORKS_PLATFORM "osx")
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(OPENFRAMEWORKS_PLATFORM "linuxarm64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armv7l")
            set(OPENFRAMEWORKS_PLATFORM "linuxarmv7l")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv6|armv6l")
            set(OPENFRAMEWORKS_PLATFORM "linuxarmv6l")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
            set(OPENFRAMEWORKS_PLATFORM "linux64")
        else()
            set(OPENFRAMEWORKS_PLATFORM "linux")
        endif()
    endif()
endif()
set(OPENFRAMEWORKS_PLATFORM "${OPENFRAMEWORKS_PLATFORM}" CACHE STRING "openFrameworks platform lib directory (e.g., osx, linuxarmv7l)")

if(NOT OPENFRAMEWORKS_LIB_DIR)
    set(OPENFRAMEWORKS_LIB_DIR "${OPENFRAMEWORKS_DIR}/libs/openFrameworksCompiled/lib/${OPENFRAMEWORKS_PLATFORM}")
endif()
set(OPENFRAMEWORKS_LIB_DIR "${OPENFRAMEWORKS_LIB_DIR}" CACHE PATH "Path to openFrameworks compiled lib directory")

if(NOT EXISTS "${OPENFRAMEWORKS_LIB_DIR}/libopenFrameworks.a")
    message(FATAL_ERROR "Could not find libopenFrameworks.a at ${OPENFRAMEWORKS_LIB_DIR}. Check OPENFRAMEWORKS_PLATFORM/OPENFRAMEWORKS_LIB_DIR.")
endif()

set(OPENFRAMEWORKS_INCLUDE_DIRS
    ${OPENFRAMEWORKS_DIR}/libs
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/3d
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/app
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/communication
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/events
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/gl
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/graphics
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/math
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/sound
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/types
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/utils
    ${OPENFRAMEWORKS_DIR}/libs/openFrameworks/video
    ${OPENFRAMEWORKS_DIR}/libs/glew/include
    ${OPENFRAMEWORKS_DIR}/libs/FreeImage/include
    ${OPENFRAMEWORKS_DIR}/libs/freetype/include
    ${OPENFRAMEWORKS_DIR}/libs/freetype/include/freetype
    ${OPENFRAMEWORKS_DIR}/libs/tess2/include
    ${OPENFRAMEWORKS_DIR}/libs/cairo/include
    ${OPENFRAMEWORKS_DIR}/libs/rtAudio/include
    ${OPENFRAMEWORKS_DIR}/libs/glfw/include
    ${OPENFRAMEWORKS_DIR}/libs/utf8/include
    ${OPENFRAMEWORKS_DIR}/libs/json/include
    ${OPENFRAMEWORKS_DIR}/libs/glm/include
    ${OPENFRAMEWORKS_DIR}/libs/curl/include
    ${OPENFRAMEWORKS_DIR}/libs/openssl/include
    ${OPENFRAMEWORKS_DIR}/libs/uriparser/include
    ${OPENFRAMEWORKS_DIR}/libs/pugixml/include
    ${OPENFRAMEWORKS_DIR}/libs/brotli/include
)

add_library(openframeworks STATIC IMPORTED)
set_target_properties(openframeworks PROPERTIES
    IMPORTED_LOCATION "${OPENFRAMEWORKS_LIB_DIR}/libopenFrameworks.a"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENFRAMEWORKS_INCLUDE_DIRS}"
)

if(APPLE)
    set(OPENFRAMEWORKS_THIRDPARTY_LIBS
        ${OPENFRAMEWORKS_DIR}/libs/FreeImage/lib/macos/FreeImage.xcframework/macos-arm64_x86_64/FreeImage.a
        ${OPENFRAMEWORKS_DIR}/libs/freetype/lib/macos/freetype.xcframework/macos-arm64_x86_64/libfreetype.a
        ${OPENFRAMEWORKS_DIR}/libs/glew/lib/macos/glew.xcframework/macos-arm64_x86_64/libGLEW.a
        ${OPENFRAMEWORKS_DIR}/libs/glfw/lib/macos/glfw.xcframework/macos-arm64_x86_64/libglfw3.a
        ${OPENFRAMEWORKS_DIR}/libs/libpng/lib/macos/libpng.xcframework/macos-arm64_x86_64/libpng.a
        ${OPENFRAMEWORKS_DIR}/libs/zlib/lib/macos/zlib.xcframework/macos-arm64_x86_64/zlib.a
        ${OPENFRAMEWORKS_DIR}/libs/cairo/lib/macos/cairo.xcframework/macos-arm64_x86_64/libcairo.a
        ${OPENFRAMEWORKS_DIR}/libs/pixman/lib/macos/pixman.xcframework/macos-arm64_x86_64/libpixman-1.a
        ${OPENFRAMEWORKS_DIR}/libs/tess2/lib/macos/tess2.xcframework/macos-arm64_x86_64/libtess2.a
        ${OPENFRAMEWORKS_DIR}/libs/uriparser/lib/macos/uriparser.xcframework/macos-arm64_x86_64/uriparser.a
        ${OPENFRAMEWORKS_DIR}/libs/pugixml/lib/macos/pugixml.xcframework/macos-arm64_x86_64/libpugixml.a
        ${OPENFRAMEWORKS_DIR}/libs/curl/lib/macos/curl.xcframework/macos-arm64_x86_64/curl.a
        ${OPENFRAMEWORKS_DIR}/libs/openssl/lib/macos/openssl.xcframework/macos-arm64_x86_64/openssl.a
        ${OPENFRAMEWORKS_DIR}/libs/brotli/lib/macos/brotli.xcframework/macos-arm64_x86_64/brotli.a
        ${OPENFRAMEWORKS_DIR}/libs/fmt/lib/macos/fmt.xcframework/macos-arm64_x86_64/libfmt.a
        ${OPENFRAMEWORKS_DIR}/libs/rtAudio/lib/macos/rtAudio.xcframework/macos-arm64_x86_64/librtaudio.a
    )

    set(OPENFRAMEWORKS_FRAMEWORKS
        Accelerate
        AppKit
        ApplicationServices
        AudioToolbox
        AVFoundation
        Cocoa
        CoreAudio
        CoreFoundation
        CoreMedia
        CoreServices
        CoreVideo
        Foundation
        IOKit
        OpenGL
        QuartzCore
        Security
        SystemConfiguration
        Metal
    )

    set(OPENFRAMEWORKS_FRAMEWORK_OPTIONS "")
    foreach(fw ${OPENFRAMEWORKS_FRAMEWORKS})
        list(APPEND OPENFRAMEWORKS_FRAMEWORK_OPTIONS "-Wl,-framework,${fw}")
    endforeach()
else()
    set(OPENFRAMEWORKS_THIRDPARTY_LIBS "")
    set(OPENFRAMEWORKS_FRAMEWORK_OPTIONS "")
endif()

set(OPENFRAMEWORKS_EXTRA_LIBS "" CACHE STRING "Extra libraries to link with openFrameworks (semicolon-separated list)")

add_library(renderer_state
    ${RENDERER_SRC_DIR}/RenderState.cpp
    ${RENDERER_SRC_DIR}/RenderState.h
)

target_include_directories(renderer_state
    PUBLIC
        ${RENDERER_SRC_DIR}
)

target_link_libraries(renderer_state
    PUBLIC
        projection_core
        openframeworks
        ${OPENFRAMEWORKS_THIRDPARTY_LIBS}
        ${OPENFRAMEWORKS_EXTRA_LIBS}
)

target_compile_features(renderer_state PUBLIC cxx_std_17)

add_library(renderer_net
    ${RENDERER_SRC_DIR}/net/RendererClient.cpp
    ${RENDERER_SRC_DIR}/net/RendererClient.h
    ${RENDERER_SRC_DIR}/net/RendererServer.cpp
    ${RENDERER_SRC_DIR}/net/RendererServer.h
)

target_include_directories(renderer_net
    PUBLIC
        ${RENDERER_SRC_DIR}
)

target_link_libraries(renderer_net
    PUBLIC
        projection_core
)

target_compile_features(renderer_net PUBLIC cxx_std_17)

add_executable(renderer_default
    ${RENDERER_SRC_DIR}/main.cpp
    ${RENDERER_SRC_DIR}/ofApp.cpp
    ${RENDERER_SRC_DIR}/ofApp.h
    ${RENDERER_SRC_DIR}/util/ShutdownUtils.cpp
    ${RENDERER_SRC_DIR}/util/ShutdownUtils.h
    ${RENDERER_SRC_DIR}/util/InteractionUtils.cpp
    ${RENDERER_SRC_DIR}/util/InteractionUtils.h
)

target_include_directories(renderer_default
    PRIVATE
        ${RENDERER_SRC_DIR}
)

target_link_libraries(renderer_default
    PRIVATE
        renderer_net
        renderer_state
        projection_core
        openframeworks
        ${OPENFRAMEWORKS_THIRDPARTY_LIBS}
        ${OPENFRAMEWORKS_EXTRA_LIBS}
)

target_compile_features(renderer_default PRIVATE cxx_std_17)
target_link_options(renderer_default PRIVATE ${OPENFRAMEWORKS_FRAMEWORK_OPTIONS})

# Copy shader data files to the output directory where the renderer can find them
set(RENDERER_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
if(EXISTS ${RENDERER_DATA_DIR})
    add_custom_command(TARGET renderer_default POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RENDERER_DATA_DIR}
            $<TARGET_FILE_DIR:renderer_default>/data
        COMMENT "Copying renderer data files (shaders, etc.)"
    )
endif()

add_executable(renderer_hello
    ${RENDERER_SRC_DIR}/hello_main.cpp
    ${RENDERER_SRC_DIR}/util/ShutdownUtils.cpp
    ${RENDERER_SRC_DIR}/util/ShutdownUtils.h
)

target_include_directories(renderer_hello
    PRIVATE
        ${RENDERER_SRC_DIR}
)

target_compile_features(renderer_hello PRIVATE cxx_std_17)
target_link_libraries(renderer_hello PRIVATE openframeworks ${OPENFRAMEWORKS_THIRDPARTY_LIBS} ${OPENFRAMEWORKS_EXTRA_LIBS})
target_link_options(renderer_hello PRIVATE ${OPENFRAMEWORKS_FRAMEWORK_OPTIONS})

add_executable(renderer_default_tests
    ${RENDERER_SRC_DIR}/util/InteractionUtils.cpp
    ${RENDERER_SRC_DIR}/util/InteractionUtils.h
    ${RENDERER_SRC_DIR}/util/ShutdownUtils.cpp
    ${RENDERER_SRC_DIR}/util/ShutdownUtils.h
    tests/RendererClient_test.cpp
    tests/RendererServer_test.cpp
    tests/RenderState_test.cpp
    tests/InteractionUtils_test.cpp
    tests/ShutdownUtils_test.cpp
)

target_link_libraries(renderer_default_tests
    PRIVATE
        renderer_net
        renderer_state
        projection_core
        Catch2::Catch2WithMain
        openframeworks
        ${OPENFRAMEWORKS_THIRDPARTY_LIBS}
        ${OPENFRAMEWORKS_EXTRA_LIBS}
)

target_include_directories(renderer_default_tests
    PRIVATE
        ${RENDERER_SRC_DIR}
        ${CMAKE_SOURCE_DIR}/core/third_party/catch2/include
)

target_compile_features(renderer_default_tests PRIVATE cxx_std_17)
target_link_options(renderer_default_tests PRIVATE ${OPENFRAMEWORKS_FRAMEWORK_OPTIONS})

include(CTest)
add_test(NAME renderer_default_tests COMMAND renderer_default_tests)
