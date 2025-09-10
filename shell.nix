{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.pkgsCross.riscv64-embedded.buildPackages.gcc
    pkgs.pkgsCross.riscv64-embedded.buildPackages.binutils
    pkgs.qemu
  ];

  shellHook = ''
    export PATH=${pkgs.pkgsCross.riscv64-embedded.buildPackages.gcc}/bin${pkgs.pkgsCross.riscv64-embedded.buildPackages.binutils}/bin:$PATH
    export TOOLPREFIX=riscv64-none-elf-
  '';
}
