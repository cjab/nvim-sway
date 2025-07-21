{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let pkgs = import nixpkgs { inherit system; };
      in {
        packages = {
          nvim-sway = pkgs.callPackage ./nix/nvim-sway.nix { };
        };

        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            stdenv
            cjson
            msgpack-c
            python312Packages.compiledb
            pkg-config
            gdb
          ];

          shellHook = ''
            set -e
            compiledb -n make
          '';
        };
      }
    );
}
