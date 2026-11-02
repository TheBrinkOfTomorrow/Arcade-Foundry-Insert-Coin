# CMake toolchain for 64-bit ARM Linux (Raspberry Pi 4/5, Orange Pi 5,
# ODROID-N2) cross-built on an x86-64 Debian or Ubuntu with the arm64
# multiarch packages (see Dockerfile). Generic armv8-a: no -mcpu, so one
# build runs on every board.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# pkg-config (SDL's video and audio backends): the arm64 .pc files
set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig)
# Test programs CMake runs while configuring
find_program(QEMU_AARCH64 qemu-aarch64)
if(QEMU_AARCH64)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${QEMU_AARCH64} -L /usr/aarch64-linux-gnu)
endif()
