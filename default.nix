{
  lib,
  stdenv,
  pkg-config,
  xorg,
  freetype,
  fontconfig,
  makeDesktopItem,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "nwm";
  version = "0.1.0";

  src = ./.;

  nativeBuildInputs = [
    pkg-config
  ];

  buildInputs = [
    xorg.libX11
    xorg.libXft
    xorg.libXrender
    xorg.libXrandr
    freetype
    fontconfig
  ];

  preBuild = ''
    if [ ! -f src/config.hpp ]; then
      cp src/default-config.hpp src/config.hpp
      echo "Copied default-config.hpp to config.hpp"
    fi
  '';

  makeFlags = [
    "PREFIX=$(out)"
    "CC=${stdenv.cc.targetPrefix}c++"
  ];

  postInstall = let
    nwmDesktopItem = makeDesktopItem rec {
      name = finalAttrs.pname;
      exec = name;
      desktopName = name;
      comment = finalAttrs.meta.description;
    };
  in ''
    install -Dt $out/share/xsessions ${nwmDesktopItem}/share/applications/nwm.desktop
  '';

  passthru.providedSessions = ["nwm"];

  meta = with lib; {
    description = "A scrollable tiling window manager for X11";
    homepage = "https://github.com/xsoder/nwm";
    license = licenses.mit;
    platforms = platforms.linux;
    mainProgram = "nwm";
  };
})
