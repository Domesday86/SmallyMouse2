## Synopsis

SmallyMouse2 is the AT90USB1287 firmware source code and KiCAD schematic/PCB design for the SmallyMouse2 project.

The schematic and PCB design in `KiCAD/` are in the KiCad 10 file format and require KiCad 10 or later to open. The original KiCad 4 files are available in the git history (commit `66cf90d` and earlier).

## Motivation

SmallyMouse2 is a project that creates a USB mouse adaptor for retro computers that use quadrature mouse input including the Acorn BBC Micro, Acorn Master series, Commodore Amiga, Atari ST, busmouse compatible computers and many more.  SmallyMouse2 provides both a generic mouse output header (for attaching mouse to retro computer cables) and an IDC connector suitable for use with Acorn 8-bit user-ports.  SmallyMouse2 also features a configurable quadrature rate limiter that prevents VIA overrun when in use with slower 8-bit machines.

SmallyMouse2 supports both JTAG and USB bootloader programming.  The AT90USB1287 is pre-programmed by Atmel with the (FLIP) DFU bootloader; this bootloader is recognised by Atmel Studio as a programming device and can be used to flash the firmware to the board.

## Installation

The firmware is built with the open-source AVR GNU toolchain (avr-gcc, avr-binutils, avr-libc) and CMake. A Nix flake supplies the complete, pinned toolchain along with the programming tools (avrdude for the Atmel-ICE, dfu-programmer for the USB bootloader).

### Building

Enter the development shell and build out-of-tree using a CMake preset:

    nix develop
    cmake --preset release          # or: cmake --preset debug
    cmake --build --preset release

The output is written to `build/release/`:

| File                | Use                                                                 |
|---------------------|---------------------------------------------------------------------|
| `SmallyMouse2.hex`  | Flash image (Intel HEX) - JTAG via avrdude, DFU via dfu-programmer  |
| `SmallyMouse2.elf`  | Full image with symbols - JTAG via Microchip Studio / debugging     |
| `SmallyMouse2.eep`  | EEPROM image (currently empty)                                      |
| `SmallyMouse2.srec` | Flash image (Motorola S-record)                                     |
| `SmallyMouse2.lss`  | Extended listing                                                    |
| `SmallyMouse2.map`  | Linker map                                                          |

Alternatively `nix build` produces the same files in `./result/` without entering a shell.

Without Nix, any avr-gcc toolchain, CMake 3.24+ and Ninja on the `PATH` will work with the same commands.

### Programming via JTAG (Atmel-ICE)

Connect the Atmel-ICE to the JTAG header, then:

    cmake --build --preset release --target flash-jtag

or, without a build tree, `nix run .#flash-jtag`. The fuses and lock bits can be read with the `read-fuses-jtag` target.

Note: programming over JTAG performs a chip erase, which also erases the factory DFU bootloader. After this the board can only be programmed over JTAG until the bootloader is restored.

### Restoring the DFU bootloader (Atmel-ICE)

To put a DFU bootloader back after a JTAG chip erase, connect the Atmel-ICE and run:

    nix run .#flash-bootloader-jtag

or, from the dev shell, `cmake --build --preset release --target flash-bootloader-jtag`. This builds the LUFA DFU bootloader from the pinned LUFA source, erases the chip, writes the bootloader and sets the high/extended fuses to the factory values (`hfuse 0x99`, `efuse 0xF3`: 8 KB boot section, JTAG enabled, HWB bootloader entry enabled). The LUFA bootloader uses the same USB ID as the factory one, so the DFU steps below work unchanged. Any application firmware is removed; reprogram it over USB afterwards.

### Programming via USB (DFU bootloader)

Put the AT90USB1287 into its factory DFU bootloader (hold HWB low while resetting), then:

    cmake --build --preset release --target flash-dfu

or `nix run .#flash-dfu`. The device is erased, programmed and then started.

### USB permissions (udev)

On Linux, programming as a normal user needs udev rules for two devices:

| Device                        | USB ID      | Used by        |
|-------------------------------|-------------|----------------|
| Atmel-ICE                     | `03eb:2141` | avrdude        |
| AT90USB1287 DFU bootloader    | `03eb:2ffb` | dfu-programmer |

The Atmel-ICE needs two rules. avrdude can reach it through libusb or through hidapi, and each uses a different device node (`usb` and `hidraw`).

On NixOS, add the rules to your system configuration:

```nix
services.udev.extraRules = ''
  # Atmel-ICE (AVR JTAG programmer) - usb node for libusb, hidraw node for hidapi
  SUBSYSTEM=="usb", ATTRS{idVendor}=="03eb", ATTRS{idProduct}=="2141", MODE="660", GROUP="users"
  SUBSYSTEM=="hidraw", ATTRS{idVendor}=="03eb", ATTRS{idProduct}=="2141", MODE="660", GROUP="users"
  # AT90USB1287 DFU bootloader
  SUBSYSTEM=="usb", ATTRS{idVendor}=="03eb", ATTRS{idProduct}=="2ffb", MODE="660", GROUP="users"
'';
```

On other distributions, put the same three lines in a file such as `/etc/udev/rules.d/70-smallymouse2.rules`. Make sure your user is in the group the rules name, or change `GROUP` to one it is in (for example `plugdev` on Debian and Ubuntu). Then reload the rules with `sudo udevadm control --reload-rules`.

On either system, unplug and replug the device after adding the rules.

Please see http://www.waitingforfriday.com/?p=827 for detailed documentation about SmallyMouse2

## Author

SmallyMouse2 is written and maintained by Simon Inns.

## License (Software)

    SmallyMouse2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SmallyMouse2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SmallyMouse2. If not, see <http://www.gnu.org/licenses/>.

## License (Hardware)

Both the schematic and the PCB design of SmallyMouse2 (i.e. the KiCAD project and files) are covered by a Creative Commons license; as with the software you are welcome (and encouraged!) to extend, re-spin and otherwise use and modify the design as allowed by the license.  However; under the terms of the Attribution-ShareAlike 4.0 International (CC BY-SA 4.0) license you are required to release your design (or redesign) under the same license.  For details of the licensing requirements please see <https://creativecommons.org/licenses/by-sa/4.0/>
