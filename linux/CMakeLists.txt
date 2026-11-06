# Arcade Foundry: Insert Coin for Linux (SDL3). Build:
#   cmake -S linux -B build/linux -G Ninja && cmake --build build/linux
# The game cores come from scripts/build_cores.py (build/cores/linux), and
# SDL from the recompiler's pinned copy (its scripts/setup.py fetches it).
cmake_minimum_required(VERSION 3.20)
project(insert_coin VERSION 1.2.0 LANGUAGES C CXX) # x.y.z, the same as apple/project.yml
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(RECOMP ${CMAKE_CURRENT_SOURCE_DIR}/../cores/arcade-foundry-cores-model12)
if(NOT EXISTS ${RECOMP}/extern/sdl3/CMakeLists.txt)
    message(FATAL_ERROR "SDL 3 is missing: run python3 scripts/build_cores.py (or the recompiler's scripts/setup.py --no-build)")
endif()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${RECOMP}/extern/sdl3 ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL SYSTEM)

# The core interface header and the tar unpacker (Arcade-Foundry-Cores-Common).
set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../cores/arcade-foundry-cores-common)
if(NOT EXISTS ${COMMON}/CMakeLists.txt)
    message(FATAL_ERROR "cores/arcade-foundry-cores-common is empty: run git submodule update --init --recursive")
endif()
add_subdirectory(${COMMON} ${CMAKE_BINARY_DIR}/common EXCLUDE_FROM_ALL)

add_executable(insert-coin main.cpp library.cpp session.cpp input.cpp render.cpp wheel.cpp art.cpp)
target_compile_definitions(insert-coin PRIVATE INSERT_COIN_VERSION="${PROJECT_VERSION}")
# The credits (CREDITS.md), built in for the credits screen and --credits.
set(CREDITS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../CREDITS.md)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CREDITS_FILE})
file(READ ${CREDITS_FILE} CREDITS_TEXT)
configure_file(credits.h.in ${CMAKE_BINARY_DIR}/generated/credits.h @ONLY)
# The games' histories (history/<romset>.md), built in for each game's page.
file(GLOB HISTORY_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../history/*.md)
set(HISTORY_ENTRIES "")
foreach(HISTORY_FILE ${HISTORY_FILES})
    get_filename_component(HISTORY_SET ${HISTORY_FILE} NAME_WE)
    file(READ ${HISTORY_FILE} HISTORY_TEXT)
    string(APPEND HISTORY_ENTRIES "    {\"${HISTORY_SET}\", R\"history(${HISTORY_TEXT})history\"},\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HISTORY_FILE})
endforeach()
configure_file(history.h.in ${CMAKE_BINARY_DIR}/generated/history.h @ONLY)
target_include_directories(insert-coin PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(insert-coin PRIVATE afc::afcommon afc::afcore SDL3::SDL3-static ${CMAKE_DL_LIBS})
# Game art downloads (art.h) need libcurl (libcurl4-openssl-dev, libcurl-devel);
# without it the app builds and shows the screens from the games instead.
find_package(CURL)
if(CURL_FOUND)
    target_link_libraries(insert-coin PRIVATE CURL::libcurl)
    target_compile_definitions(insert-coin PRIVATE AF_HAVE_CURL)
else()
    message(WARNING "libcurl not found: this build won't download game art (install libcurl4-openssl-dev or libcurl-devel)")
endif()
# Next to the executable: cores/ (a link to build/cores/linux), found first.
# The Mac build (a test of this interface on macOS, cmake -S linux -B
# build/sdl-mac) uses the Apple app's cores, build/cores/macosx.
if(APPLE)
    set(CORES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../build/cores/macosx)
else()
    set(CORES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../build/cores/linux)
endif()
add_custom_command(TARGET insert-coin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CORES_DIR} $<TARGET_FILE_DIR:insert-coin>/cores)
