{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    clang
    libclang
    glib
    gsl
    pkg-config
    bear
  ];

  shellHook = ''
    export LIBCLANG_PATH="${pkgs.libclang.lib}/lib"
  '';
}
