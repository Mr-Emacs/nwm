{
  description = "NWM - A scrollable tiling window manager";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    let
      nixosModulesOutput = {
        nixosModules.default = { config, lib, pkgs, ... }:
          with lib;
          let
            cfg = config.services.xserver.windowManager.nwm;
          in {
            options.services.xserver.windowManager.nwm = {
              enable = mkEnableOption "nwm window manager";
              package = mkOption {
                type = types.package;
                default = self.packages.${pkgs.system}.default;
                description = "nwm package to use";
              };
            };

            config = mkIf cfg.enable {
              services.xserver.windowManager.session = [{
                name = "nwm";
                start = ''
                  ${cfg.package}/bin/nwm &
                  waitPID=$!
                '';
              }];
              environment.systemPackages = [ cfg.package ];
            };
          };
      };

      systemOutputs = flake-utils.lib.eachDefaultSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };

          nwm = pkgs.callPackage ./default.nix { };

        in
        {
          packages = {
            default = nwm;
            nwm = nwm;
          };

          devShells.default = pkgs.mkShell {
            name = "nwm-dev";

            packages = with pkgs; [
              pkg-config
              gcc
              gnumake
              git
              xorg.libX11
              xorg.libXft
              xorg.libXrender
              fontconfig
              freetype
              nerd-fonts.iosevka
              gdb
              valgrind
              xorg.xorgserver
              xorg.xinit
              xorg.xev
              xorg.xdpyinfo
              feh
              st
              dmenu
            ];

            XINERAMA = "1";
            FREETYPEINC = "${pkgs.freetype.dev}/include/freetype2";
            X11INC = "${pkgs.xorg.libX11.dev}/include";
            X11LIB = "${pkgs.xorg.libX11}/lib";
          };

          devShells.minimal = pkgs.mkShell {
            packages = with pkgs; [
              pkg-config
              xorg.libX11
              xorg.libXft
              xorg.libXrender
              fontconfig
              freetype
              gcc
              gnumake
            ];
          };
        }
      );
    in
      nixosModulesOutput // systemOutputs;
}
