{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    clang
    glib
    gsl
    pkg-config
    bear
  ];
}
