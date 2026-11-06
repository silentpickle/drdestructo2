cmake_minimum_required(VERSION 3.24)
project(DrDestructoWeb LANGUAGES C CXX)

if(EMSCRIPTEN)
    execute_process(COMMAND "${CMAKE_C_COMPILER}" --version
        OUTPUT_VARIABLE em_version COMMAND_ERROR_IS_FATAL ANY)
endif()
if(NOT EMSCRIPTEN OR NOT em_version MATCHES " 4[.]0[.]23[ \r\n]")
    message(FATAL_ERROR "Configure with emcmake from emsdk 4.0.23. See README.md.")
endif()

include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(FFMPEG ffmpeg REQUIRED)
find_program(OPENMPT openmpt123 REQUIRED)

# Emscripten ports provide headers and static libraries to Allegro's CMake checks.
set(port_flags "-sUSE_SDL=2 -sUSE_LIBPNG=1 -sUSE_FREETYPE=1")
string(APPEND CMAKE_C_FLAGS " ${port_flags}")
string(APPEND CMAKE_CXX_FLAGS " ${port_flags}")
execute_process(COMMAND em-config CACHE
    OUTPUT_VARIABLE em_cache OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(SDL2_INCLUDE_DIR "${em_cache}/sysroot/include/SDL2" CACHE PATH "")
set(SDL2_LIBRARY "${em_cache}/sysroot/lib/wasm32-emscripten/libSDL2.a" CACHE FILEPATH "")
set(ALLEGRO_SDL ON CACHE BOOL "")
set(SHARED OFF CACHE BOOL "")
set(WANT_MONOLITH ON CACHE BOOL "")
set(WANT_WAIT_EVENT_SLEEP ON CACHE BOOL "")
foreach(feature AUDIO VIDEO NATIVE_DIALOG PHYSFS DOCS DEMO EXAMPLES TESTS IMAGE_JPG IMAGE_WEBP IMAGE_FREEIMAGE)
    set(WANT_${feature} OFF CACHE BOOL "")
endforeach()

FetchContent_Declare(allegro
    URL https://github.com/liballeg/allegro5/releases/download/5.2.11.3/allegro-5.2.11.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL_HASH SHA256=aba4679a5b1f2bf62482eba6e8814a94de7ffc86de5f8587ba199fcc61b4a04f)
FetchContent_Declare(lua
    URL https://www.lua.org/ftp/lua-5.5.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL_HASH SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce)
FetchContent_MakeAvailable(allegro lua)
# Allegro's monolith passes an unquoted build interface. Use absolute build paths
# so a FetchContent download under the build directory can be exported by CMake.
set_property(TARGET allegro_monolith PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${allegro_SOURCE_DIR}/include;${allegro_BINARY_DIR}/include>")

file(GLOB lua_sources "${lua_SOURCE_DIR}/src/*.c")
list(FILTER lua_sources EXCLUDE REGEX "/(lua|luac|ltests)\\.c$")
add_library(dd2_lua STATIC ${lua_sources})
target_include_directories(dd2_lua SYSTEM PUBLIC "${lua_SOURCE_DIR}/src")

set(root "${CMAKE_CURRENT_LIST_DIR}/..")
file(GLOB game_sources CONFIGURE_DEPENDS "${root}/src/*.cpp" "${root}/src/Engine/*.cpp")
list(FILTER game_sources EXCLUDE REGEX "/(AIProtoManager|AllegroSound5)\\.cpp$")
add_executable(DrDestructo2 ${game_sources} LuaAI.cpp WebSound.cpp
    "${root}/libs/src/tinyxml/tinystr.cpp"
    "${root}/libs/src/tinyxml/tinyxml.cpp"
    "${root}/libs/src/tinyxml/tinyxmlerror.cpp"
    "${root}/libs/src/tinyxml/tinyxmlparser.cpp")
target_compile_features(DrDestructo2 PRIVATE cxx_std_17)
target_compile_definitions(DrDestructo2 PRIVATE TIXML_USE_STL HAS_STD_SHARED_PTR ALLEGRO_UNSTABLE
    $<$<CONFIG:Debug>:_DEBUG>)
target_include_directories(DrDestructo2 PRIVATE
    "${root}/src" "${root}/src/Engine" "${root}/src/tinyxml" "${CMAKE_CURRENT_LIST_DIR}")
foreach(addon image font ttf primitives)
    target_include_directories(DrDestructo2 SYSTEM PRIVATE "${allegro_SOURCE_DIR}/addons/${addon}")
endforeach()
target_link_libraries(DrDestructo2 PRIVATE allegro_monolith dd2_lua)

set(dist "${root}/dist" CACHE PATH "Static browser output directory")
set(stage "${CMAKE_CURRENT_BINARY_DIR}/game-assets")
file(GLOB_RECURSE data_files CONFIGURE_DEPENDS "${root}/data/*")
add_custom_command(OUTPUT "${stage}/prepared" "${dist}/index.html"
    BYPRODUCTS "${dist}/game.js" "${dist}/audio.json" "${dist}/favicon.ico"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/stage_assets.py"
        "${root}" "${stage}" "${dist}" "${FFMPEG}" "${OPENMPT}" "${allegro_SOURCE_DIR}" "${lua_SOURCE_DIR}"
    DEPENDS ${data_files} stage_assets.py index.html game.js BalsamiqSans-Regular.ttf BalsamiqSans-OFL.txt
    VERBATIM)
add_custom_target(web_assets DEPENDS "${stage}/prepared" "${dist}/index.html")
add_dependencies(DrDestructo2 web_assets)
set_target_properties(DrDestructo2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${dist}" SUFFIX ".js"
    LINK_DEPENDS "${stage}/prepared")
target_link_options(DrDestructo2 PRIVATE
    "SHELL:--preload-file \"${stage}/Data@/Data\""
    -lidbfs.js -sFORCE_FILESYSTEM=1 -sALLOW_MEMORY_GROWTH=1 -sFULL_ES2=1
    -sMODULARIZE=1 -sEXPORT_NAME=createDestructo -sENVIRONMENT=web
    -sINVOKE_RUN=0 "-sEXPORTED_RUNTIME_METHODS=callMain,FS,IDBFS"
    "SHELL:-sEXPORTED_FUNCTIONS=_main,_dd2_set_paused,_dd2_web_action,_dd2_web_tap")
