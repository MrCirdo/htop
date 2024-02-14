{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.11";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = {
    nixpkgs,
    flake-utils,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {inherit system;};
        libunwindDebug = pkgs.libunwind.overrideAttrs(old: {
          configureFlags = old.configureFlags ++ ["--enable-debug"];
        });

      in rec {
        formatter = nixpkgs.legacyPackages.${system}.alejandra;
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            autoconf
            ncurses
          ] ++ [libunwindDebug];
        };
      }
    );
}
