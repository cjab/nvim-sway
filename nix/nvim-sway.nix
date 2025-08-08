{ lib
, stdenv
, cjson
, msgpack-c
, pkg-config
}:

stdenv.mkDerivation {
  pname = "nvim-sway";
  version = "0.2.2";
  src = lib.cleanSource ../.;
  nativeBuildInputs = [ cjson msgpack-c pkg-config ];

  installPhase = ''
    mkdir -p $out/bin
    cp nvim-sway $out/bin/
    mkdir -p $out/share/
    cp -r man $out/share/
  '';

  meta = with lib; {
    description = "nvim-sway";
    homepage = "https://github.com/cjab/nvim-sway";
    license = licenses.gpl3Only;
    platforms = with platforms; linux;
    maintainers = with maintainers; [ cjab ];
  };
}
