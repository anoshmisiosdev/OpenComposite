# CMake toolchain: cross-compile a Windows x86_64 PE (openvr_api.dll) with mingw-w64.
#
# This is the toolchain the oxrsys-winebridge project builds OpenComposite with.
# The MSVC<->GCC x64 ABI trampolines in this fork are GCC-specific, so the DLL
# MUST be built with mingw-w64 g++ (not MSVC) to be ABI-correct against the
# MSVC-compiled games it is dropped into. Works on both an Ubuntu runner
# (/usr/bin/x86_64-w64-mingw32-*) and Homebrew mac (/opt/homebrew/bin/...).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc     REQUIRED)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++     REQUIRED)
find_program(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres REQUIRED)

# Anchor CMake's find_* at the MinGW sysroot so lookups do not escape to the
# host. Derived from the compiler path so it works regardless of install prefix.
get_filename_component(MINGW_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(MINGW_ROOT    "${MINGW_BIN_DIR}/.."  ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH "${MINGW_ROOT}/${TOOLCHAIN_PREFIX}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Without this, CMake checks runtime deps against the host and aborts on the PE
# binary with "file unknown".
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
