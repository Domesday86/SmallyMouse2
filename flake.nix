{
  description = "SmallyMouse2 - USB mouse to quadrature adaptor firmware (AT90USB1287)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

    # LUFA USB stack (no flake of its own). Keep the rev in step with the
    # FetchContent fallback in CMakeLists.txt.
    lufa = {
      url = "github:abcminiuser/lufa/90d65ba059d91078a34b9c26c8772ee14b556a13";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, lufa }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # Bare-metal AVR toolchain: avr-gcc (wrapped with avr-libc) and avr-binutils.
      avrToolchain = pkgs: [
        pkgs.pkgsCross.avr.buildPackages.gcc
        pkgs.pkgsCross.avr.buildPackages.binutils
      ];

      # avrdude drives the Atmel-ICE over JTAG; dfu-programmer talks to the
      # factory Atmel (FLIP) DFU bootloader over USB.
      programmers = pkgs: [
        pkgs.avrdude
        pkgs.dfu-programmer
      ];
    in
    {
      packages = forAllSystems (pkgs: rec {
        firmware = pkgs.stdenvNoCC.mkDerivation {
          pname = "smallymouse2-firmware";
          version = self.shortRev or self.dirtyShortRev or "dev";

          src = nixpkgs.lib.fileset.toSource {
            root = ./.;
            fileset = nixpkgs.lib.fileset.unions [
              ./CMakeLists.txt
              ./cmake
              ./SmallyMouse2
            ];
          };

          nativeBuildInputs = [ pkgs.cmake pkgs.ninja ] ++ avrToolchain pkgs;

          cmakeFlags = [
            "-DCMAKE_TOOLCHAIN_FILE=cmake/avr-gcc.cmake"
            "-DFETCHCONTENT_SOURCE_DIR_LUFA=${lufa}"
          ];

          dontFixup = true;
        };
        default = firmware;
      });

      apps = forAllSystems (pkgs:
        let
          firmware = self.packages.${pkgs.stdenv.hostPlatform.system}.firmware;
          mkApp = name: runtimeInputs: text: {
            type = "app";
            program = nixpkgs.lib.getExe (pkgs.writeShellApplication { inherit name runtimeInputs text; });
          };
        in
        {
          # Atmel-ICE over JTAG. The chip erase also removes the factory DFU bootloader.
          flash-jtag = mkApp "smallymouse2-flash-jtag" [ pkgs.avrdude ] ''
            avrdude -c atmelice -P usb -p usb1287 -U flash:w:${firmware}/SmallyMouse2.hex:i "$@"
          '';

          # Factory Atmel (FLIP) DFU bootloader over USB.
          flash-dfu = mkApp "smallymouse2-flash-dfu" [ pkgs.dfu-programmer ] ''
            dfu-programmer at90usb1287 erase --force
            dfu-programmer at90usb1287 flash ${firmware}/SmallyMouse2.hex
            dfu-programmer at90usb1287 launch
          '';
        });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShellNoCC {
          packages = [ pkgs.cmake pkgs.ninja ] ++ avrToolchain pkgs ++ programmers pkgs;
          LUFA_SOURCE_DIR = "${lufa}";
        };
      });
    };
}
