{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.pkgsCross.riscv64-embedded.buildPackages.gcc
    pkgs.pkgsCross.riscv64-embedded.buildPackages.binutils
    pkgs.qemu
  ];

  shellHook = ''
    echo "RISC-V xv6 development environment loaded."
    echo "Available toolchain: riscv64-unknown-elf-gcc, ld, objcopy, etc."
  '';
}
