cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/avr-gcc.cmake")
endif()

project(SmallyMouse2 LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-tree builds are not supported; use a preset, e.g. `cmake --preset release`")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release or Debug)" FORCE)
endif()

# ---------------------------------------------------------------------------
# Target hardware
# ---------------------------------------------------------------------------

set(AVR_MCU      at90usb1287 CACHE STRING "avr-gcc -mmcu device")
set(AVRDUDE_PART usb1287     CACHE STRING "avrdude -p part id")
set(DFU_TARGET   at90usb1287 CACHE STRING "dfu-programmer target")
set(F_CPU        16000000UL)

set(AVRDUDE_PROGRAMMER atmelice CACHE STRING "avrdude -c programmer id (atmelice = Atmel-ICE over JTAG)")
set(AVRDUDE_PORT       usb      CACHE STRING "avrdude -P port")

# ---------------------------------------------------------------------------
# Firmware
# ---------------------------------------------------------------------------

set(FW_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/SmallyMouse2)
set(LUFA_DIR ${FW_DIR}/src/LUFA/LUFA)

add_executable(SmallyMouse2
    ${FW_DIR}/main.c
    ${FW_DIR}/ConfigDescriptor.c

    # LUFA (host mode only, see src/config/LUFAConfig.h)
    ${LUFA_DIR}/Drivers/Peripheral/AVR8/Serial_AVR8.c
    ${LUFA_DIR}/Drivers/USB/Core/AVR8/Host_AVR8.c
    ${LUFA_DIR}/Drivers/USB/Core/AVR8/Pipe_AVR8.c
    ${LUFA_DIR}/Drivers/USB/Core/AVR8/PipeStream_AVR8.c
    ${LUFA_DIR}/Drivers/USB/Core/AVR8/USBController_AVR8.c
    ${LUFA_DIR}/Drivers/USB/Core/AVR8/USBInterrupt_AVR8.c
    ${LUFA_DIR}/Drivers/USB/Core/ConfigDescriptors.c
    ${LUFA_DIR}/Drivers/USB/Core/Events.c
    ${LUFA_DIR}/Drivers/USB/Core/HostStandardReq.c
    ${LUFA_DIR}/Drivers/USB/Core/USBTask.c
)

set_target_properties(SmallyMouse2 PROPERTIES
    SUFFIX       .elf
    C_STANDARD   99
    C_EXTENSIONS ON
)

target_include_directories(SmallyMouse2 PRIVATE
    ${FW_DIR}/src
    ${FW_DIR}/src/LUFA
    ${FW_DIR}/src/config
)

target_compile_definitions(SmallyMouse2 PRIVATE
    ARCH=ARCH_AVR8
    BOARD=BOARD_NONE
    F_CPU=${F_CPU}
    F_USB=${F_CPU}
    USE_LUFA_CONFIG_HEADER
    $<$<CONFIG:Debug>:DEBUG>
)

target_compile_options(SmallyMouse2 PRIVATE
    -mmcu=${AVR_MCU}
    -Wall
    -funsigned-char
    -funsigned-bitfields
    -fpack-struct
    -fshort-enums
    -ffunction-sections
    -fdata-sections
    -fno-strict-aliasing
    -fno-jump-tables
    -mrelax
)

target_link_options(SmallyMouse2 PRIVATE
    -mmcu=${AVR_MCU}
    -Wl,--gc-sections
    -Wl,--relax
    -Wl,-Map=$<TARGET_FILE_DIR:SmallyMouse2>/SmallyMouse2.map
    -Wl,--print-memory-usage
)

target_link_libraries(SmallyMouse2 PRIVATE m)

# LUFA's weak event stubs alias functions of differing signatures, which newer
# GCC flags; the aliasing is intentional and the vendored LUFA is left as-is.
set_source_files_properties(${LUFA_DIR}/Drivers/USB/Core/Events.c PROPERTIES
    COMPILE_OPTIONS "-Wno-attributes;-Wno-missing-attributes;-Wno-attribute-alias"
)

set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG   "-O1 -g3")

# ---------------------------------------------------------------------------
# Programming images
#   SmallyMouse2.hex  - flash image (JTAG via avrdude and DFU via dfu-programmer)
#   SmallyMouse2.eep  - EEPROM image
#   SmallyMouse2.srec - flash image, Motorola S-record
#   SmallyMouse2.lss  - extended listing
#   SmallyMouse2.elf  - full image with debug info (Atmel Studio / Microchip Studio can program this directly)
# ---------------------------------------------------------------------------

set(FW_ELF  $<TARGET_FILE:SmallyMouse2>)
set(FW_HEX  ${CMAKE_CURRENT_BINARY_DIR}/SmallyMouse2.hex)
set(FW_EEP  ${CMAKE_CURRENT_BINARY_DIR}/SmallyMouse2.eep)
set(FW_SREC ${CMAKE_CURRENT_BINARY_DIR}/SmallyMouse2.srec)
set(FW_LSS  ${CMAKE_CURRENT_BINARY_DIR}/SmallyMouse2.lss)

set(FLASH_SECTIONS -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures)

add_custom_command(TARGET SmallyMouse2 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex  ${FLASH_SECTIONS} ${FW_ELF} ${FW_HEX}
    COMMAND ${CMAKE_OBJCOPY} -O srec  ${FLASH_SECTIONS} ${FW_ELF} ${FW_SREC}
    COMMAND ${CMAKE_OBJCOPY} -O ihex  -j .eeprom --set-section-flags=.eeprom=alloc,load
                                      --change-section-lma .eeprom=0 --no-change-warnings
                                      ${FW_ELF} ${FW_EEP}
    COMMAND ${CMAKE_OBJDUMP} -h -S ${FW_ELF} > ${FW_LSS}
    COMMAND ${CMAKE_SIZE} ${FW_ELF}
    BYPRODUCTS ${FW_HEX} ${FW_SREC} ${FW_EEP} ${FW_LSS}
    VERBATIM
)

install(FILES ${FW_ELF} ${FW_HEX} ${FW_EEP} ${FW_SREC} ${FW_LSS}
              ${CMAKE_CURRENT_BINARY_DIR}/SmallyMouse2.map
        DESTINATION .)

# ---------------------------------------------------------------------------
# Flashing targets
# ---------------------------------------------------------------------------

find_program(AVRDUDE avrdude)
find_program(DFU_PROGRAMMER dfu-programmer)

if(AVRDUDE)
    # Atmel-ICE over JTAG. Note that the chip erase this performs also removes
    # the factory DFU bootloader.
    add_custom_target(flash-jtag
        COMMAND ${AVRDUDE} -c ${AVRDUDE_PROGRAMMER} -P ${AVRDUDE_PORT} -p ${AVRDUDE_PART}
                -U flash:w:${FW_HEX}:i
        DEPENDS SmallyMouse2
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(read-fuses-jtag
        COMMAND ${AVRDUDE} -c ${AVRDUDE_PROGRAMMER} -P ${AVRDUDE_PORT} -p ${AVRDUDE_PART}
                -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h -U lock:r:-:h
        USES_TERMINAL
        VERBATIM
    )
else()
    message(STATUS "avrdude not found: flash-jtag target unavailable")
endif()

if(DFU_PROGRAMMER)
    # Factory Atmel (FLIP) DFU bootloader over USB.
    add_custom_target(flash-dfu
        COMMAND ${DFU_PROGRAMMER} ${DFU_TARGET} erase --force
        COMMAND ${DFU_PROGRAMMER} ${DFU_TARGET} flash ${FW_HEX}
        COMMAND ${DFU_PROGRAMMER} ${DFU_TARGET} launch
        DEPENDS SmallyMouse2
        USES_TERMINAL
        VERBATIM
    )
else()
    message(STATUS "dfu-programmer not found: flash-dfu target unavailable")
endif()
