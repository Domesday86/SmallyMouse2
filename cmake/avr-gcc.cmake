# CMake toolchain file for the bare-metal AVR 8-bit GNU toolchain (avr-gcc,
# avr-binutils, avr-libc). The flake's dev shell puts these on PATH; outside
# Nix, any avr-gcc install on PATH (or under AVR_TOOLCHAIN_PREFIX) works.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

set(AVR_TOOLCHAIN_PREFIX "" CACHE PATH "Directory holding the avr-* tools (empty: search PATH)")
if(AVR_TOOLCHAIN_PREFIX)
    set(_avr_hints HINTS "${AVR_TOOLCHAIN_PREFIX}" "${AVR_TOOLCHAIN_PREFIX}/bin")
endif()

find_program(CMAKE_C_COMPILER avr-gcc ${_avr_hints} REQUIRED)
find_program(CMAKE_ASM_COMPILER avr-gcc ${_avr_hints} REQUIRED)
find_program(CMAKE_OBJCOPY avr-objcopy ${_avr_hints} REQUIRED)
find_program(CMAKE_OBJDUMP avr-objdump ${_avr_hints} REQUIRED)
find_program(CMAKE_SIZE avr-size ${_avr_hints} REQUIRED)

# There is no hosted runtime to link a test executable against before the
# MCU is known, so let CMake's compiler checks stop at a static library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
