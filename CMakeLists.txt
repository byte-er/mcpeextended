cmake_minimum_required(VERSION 3.21)
project(MinecraftPE)

# let CPM OPTIONS override option() defaults in dependencies
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
include(cmake/CPM.cmake)

# Default to an optimised build (previously no build type meant -O0)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing -fpermissive -w")
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing -Wno-narrowing -Wno-invalid-source-encoding -Wno-reserved-user-defined-literal")
endif()

if(EMSCRIPTEN)
    set(MC_PNG_SHARED OFF)
else()
    set(MC_PNG_SHARED ON)
endif()

if(EMSCRIPTEN)
    # wasm has no shared libraries: only build the static zlib
    CPMAddPackage(NAME "zlib" GITHUB_REPOSITORY "madler/zlib" VERSION "1.3.2"
        OPTIONS "ZLIB_BUILD_SHARED OFF" "ZLIB_BUILD_TESTING OFF" "ZLIB_BUILD_EXAMPLES OFF")
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic) # what libpng links against
    endif()
else()
    CPMAddPackage("gh:madler/zlib@1.3.2")
endif()
CPMAddPackage(
    NAME "libpng"
    GIT_REPOSITORY "https://github.com/pnggroup/libpng.git"
    GIT_TAG "v1.6.55"
    OPTIONS
        "ZLIB_ROOT ${zlib_SOURCE_DIR}"
        "ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR}"
        "PNG_TOOLS OFF"
        "PNG_TESTS OFF"
        "PNG_SHARED ${MC_PNG_SHARED}"
        "BUILD_SHARED_LIBS ${MC_PNG_SHARED}"
)

if(NOT EMSCRIPTEN)
CPMAddPackage(
    NAME "openal"
    GIT_REPOSITORY "https://github.com/kcat/openal-soft.git"
    GIT_TAG "1.25.1"
    OPTIONS
        "ALSOFT_EXAMPLES OFF"
        "ALSOFT_TESTS OFF"
        "ALSOFT_UTILS OFF"
        "BUILD_SHARED_LIBS ON"
)

CPMAddPackage(
    NAME "glfw"
    GIT_REPOSITORY "https://github.com/glfw/glfw.git"
    GIT_TAG "3.4"
    OPTIONS
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_DOCS OFF"
        "BUILD_SHARED_LIBS ON"
)
endif()

file(GLOB SOURCES 
    "src/client/*.cpp"

    "src/client/gamemode/*.cpp"
    "src/client/gui/*.cpp"
    "src/client/gui/components/*.cpp"
    "src/client/gui/screens/*.cpp"
    "src/client/gui/screens/crafting/*.cpp"
    "src/client/gui/screens/touch/*.cpp"

    "src/client/model/*.cpp"
    "src/client/model/geom/*.cpp"

    "src/client/particle/*.cpp"

    "src/client/player/*.cpp"
    "src/client/player/input/*.cpp"
    "src/client/player/input/touchscreen/*.cpp"

    "src/client/renderer/*.cpp"
    "src/client/renderer/culling/*.cpp"
    "src/client/renderer/entity/*.cpp"
    "src/client/renderer/ptexture/*.cpp"
    "src/client/renderer/tileentity/*.cpp"

    "src/client/sound/*.cpp"
    
    "src/locale/*.cpp"
    
    "src/nbt/*.cpp"

    "src/network/*.cpp"
    "src/network/command/*.cpp"
    "src/network/eagler/*.cpp"
    "src/client/skin/*.cpp"

    "src/platform/*.cpp"
    "src/platform/audio/SoundSystemAL.cpp"
    "src/platform/input/*.cpp"

    "src/raknet/**.cpp"
    
    "src/server/**.cpp"
    
    "src/util/**.cpp"

    "src/world/*.cpp"
    "src/world/phys/*.cpp"
    "src/world/entity/*.cpp"
    "src/world/entity/ai/control/*.cpp"
    "src/world/entity/animal/*.cpp"
    "src/world/entity/item/*.cpp"
    "src/world/entity/monster/*.cpp"
    "src/world/entity/npc/*.cpp"
    "src/world/entity/player/*.cpp"
    "src/world/entity/projectile/*.cpp"

    "src/world/food/*.cpp"
    "src/world/inventory/*.cpp"

    "src/world/item/*.cpp"
    "src/world/item/crafting/*.cpp"

    "src/world/level/*.cpp"
    "src/world/level/biome/*.cpp"
    "src/world/level/chunk/*.cpp"
    "src/world/level/dimension/*.cpp"
    "src/world/level/levelgen/*.cpp"
    "src/world/level/levelgen/feature/*.cpp"
    "src/world/level/levelgen/synth/*.cpp"
    "src/world/level/material/*.cpp"
    "src/world/level/pathfinder/*.cpp"
    "src/world/level/storage/*.cpp"
    "src/world/level/tile/*.cpp"
    "src/world/level/tile/entity/*.cpp"

    "src/SharedConstants.cpp"
    "src/main.cpp"
    "src/NinecraftApp.cpp"
)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "PLATFORM_GLFW")
endif()


if(PLATFORM STREQUAL "PLATFORM_WIN32")
    list(APPEND SOURCES "src/AppPlatform_win32.cpp")
endif()

if(PLATFORM STREQUAL "PLATFORM_GLFW")
    list(APPEND SOURCES "src/AppPlatform_glfw.cpp")
endif()

# Explicitly list files added after the initial glob so they are always included
list(APPEND SOURCES
    "src/client/gui/screens/ConsoleScreen.cpp"
    "src/client/gui/screens/UsernameScreen.cpp"
    "src/client/gui/screens/CreditsScreen.cpp"
)

add_executable(${PROJECT_NAME} 
    ${SOURCES}
    "glad/src/glad.c"
)

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    # portable Linux build: find the bundled .so files next to the binary
    set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "\$ORIGIN" INSTALL_RPATH "\$ORIGIN")
endif()

if(WIN32)
    set(EXTRA_LIBS "ws2_32")
    target_compile_definitions(${PROJECT_NAME} PUBLIC "_CRT_SECURE_NO_WARNINGS")
endif()

if(PLATFORM STREQUAL "PLATFORM_WIN32" OR PLATFORM STREQUAL "PLATFORM_GLFW")
    target_compile_definitions(${PROJECT_NAME} PUBLIC "PLATFORM_DESKTOP")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC 
    "${CMAKE_SOURCE_DIR}/glad/include/"
    "${CMAKE_SOURCE_DIR}/src"
    "lib/include"
)
if(NOT EMSCRIPTEN)
    target_include_directories(${PROJECT_NAME} PUBLIC "${openal_SOURCE_DIR}/include" "${glfw_SOURCE_DIR}/include")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC "OPENGL_ES" "NO_EGL" ${PLATFORM})
if(EMSCRIPTEN)
    # HTML5 / PWA build: Emscripten ports instead of the native libraries,
    # fixed function GL emulated on top of WebGL, saves in IndexedDB.
    target_compile_definitions(${PROJECT_NAME} PUBLIC NO_NETWORK)
    target_compile_options(${PROJECT_NAME} PUBLIC "-sUSE_GLFW=3" "-Wno-everything")
    target_include_directories(${PROJECT_NAME} PUBLIC "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}" "${libpng_SOURCE_DIR}" "${libpng_BINARY_DIR}")
    target_link_libraries(${PROJECT_NAME} png_static zlibstatic)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
    target_link_options(${PROJECT_NAME} PUBLIC
        "-sUSE_GLFW=3"
        "-sLEGACY_GL_EMULATION=1" "-sGL_ENABLE_GET_PROC_ADDRESS=1" "-sGL_UNSAFE_OPTS=0"
        # phones (iOS Safari) kill tabs that use too much: start small (a world needs
        # ~110 MB), grow only if needed, never past 1 GB
        "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=167772160" "-sMAXIMUM_MEMORY=1073741824" "-sMEMORY_GROWTH_GEOMETRIC_STEP=0.25" "-sSTACK_SIZE=4194304"
        "-sFORCE_FILESYSTEM=1" "-lidbfs.js" "-lwebsocket.js" "-lopenal"
        "-sEXPORTED_RUNTIME_METHODS=['FS','addRunDependency','removeRunDependency']"
        "--exclude-file" "${CMAKE_SOURCE_DIR}/data/app"
        "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html")
    option(MCPE_WEB_SINGLE_FILE "Web: one self contained .html (wasm + data embedded)" OFF)
    if(MCPE_WEB_SINGLE_FILE)
        # everything inlined: open the file straight from disk, no web server
        set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "mcpeextended")
        target_link_options(${PROJECT_NAME} PUBLIC "-sSINGLE_FILE=1"
            "--embed-file" "${CMAKE_SOURCE_DIR}/data@/data")
    else()
        target_link_options(${PROJECT_NAME} PUBLIC
            "--preload-file" "${CMAKE_SOURCE_DIR}/data@/data")
    endif()
else()
target_link_libraries(${PROJECT_NAME} zlib png_shared alsoft.common OpenAL::OpenAL glfw ${EXTRA_LIBS})
endif()

if (WIN32)
add_custom_command(
    TARGET ${PROJECT_NAME} 
    POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}> 
        COMMAND_EXPAND_LISTS
)
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_SONAME_FILE:png_shared> $<TARGET_SONAME_FILE:OpenAL> $<TARGET_SONAME_FILE:glfw> $<TARGET_SONAME_FILE:zlib> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
endif()

if(NOT EMSCRIPTEN)
add_custom_command(
    TARGET ${PROJECT_NAME} 
    POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" $<TARGET_FILE_DIR:${PROJECT_NAME}>/data
)
endif()