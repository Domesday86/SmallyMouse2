# Merge the firmware and bootloader Intel HEX images into a single image.
#
#   cmake -DFIRMWARE=<hex> -DBOOTLOADER=<hex> -DOUTPUT=<hex> -P merge-hex.cmake
#
# The end-of-file (01) and start-address (03, 05) records are dropped from both
# inputs and a single end-of-file record is written at the end. Each extended
# address record (02, 04) replaces the current base address, so the bootloader's
# own record places it correctly after the firmware's data.

foreach(var FIRMWARE BOOTLOADER OUTPUT)
    if(NOT ${var})
        message(FATAL_ERROR "merge-hex: ${var} not set")
    endif()
endforeach()

set(merged "")
foreach(input IN ITEMS "${FIRMWARE}" "${BOOTLOADER}")
    # Split on newlines ourselves; Intel HEX records never contain ';'
    file(READ "${input}" content)
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")
    foreach(line IN LISTS lines)
        if(line MATCHES "^:" AND NOT line MATCHES "^:......0[135]")
            string(APPEND merged "${line}\n")
        endif()
    endforeach()
endforeach()
string(APPEND merged ":00000001FF\n")

file(WRITE "${OUTPUT}" "${merged}")
