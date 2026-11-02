# Arcade Foundry: Insert Coin for Linux (SDL3). Build:
#   cmake -S linux -B build/linux -G Ninja && cmake --build build/linux
# The game cores come from scripts/build_cores.py (build/cores/linux), and
# SDL from the recompiler's pinned copy (its scripts/setup.py fetches it).
cmake_minimum_required(VERSION 3.20)
project(insert_coin VERSION 0.2.0 LANGUAGES C CXX) # x.y.z, the same as apple/project.yml
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

add_executable(insert-coin main.cpp)
target_include_directories(insert-coin PRIVATE ${RECOMP}/include)
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
target_link_libraries(insert-coin PRIVATE SDL3::SDL3-static ${CMAKE_DL_LIBS})
# Next to the executable: cores/ (a link to build/cores/linux), found first.
add_custom_command(TARGET insert-coin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../build/cores/linux $<TARGET_FILE_DIR:insert-coin>/cores)
