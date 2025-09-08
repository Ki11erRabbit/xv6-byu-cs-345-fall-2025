{ pkgs ? import <nixpkgs> {} }:
with import <nixpkgs> {
    crossSystem = {
        config = "riscv64-none-elf";
    };
};

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    gdb
    binutils
  ];

    nativeBuildInputs = with pkgs; [
        qemu
    ];
  
    shellHook = ''
    '';
}
