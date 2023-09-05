{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.05";
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
      in rec {
        formatter = nixpkgs.legacyPackages.${system}.alejandra;
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            autoconf
            libunwind
            ncurses
          ];
        };
      }
    );
}
