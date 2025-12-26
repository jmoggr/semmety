# somewhat inspired by:
# https://github.com/shezdy/hyprsplit/blob/main/flake.nix
{
  description = "Semmety";

  inputs = {
    hyprland = {
      url = "github:hyprwm/Hyprland/v0.52.2";
    };
  };

  outputs =
    { self, hyprland, ... }:
    let
      inherit (hyprland.inputs) nixpkgs;
      eachSystem = nixpkgs.lib.genAttrs (import hyprland.inputs.systems);
      pkgsFor = eachSystem (system: import nixpkgs { localSystem = system; });
    in
    {
      packages = eachSystem (
        system:
        let
          pkgs = pkgsFor.${system};
        in
        {
          semmety = pkgs.gcc15Stdenv.mkDerivation {
            pname = "semmety";
            version = "0.1";

            src = ./.;

            nativeBuildInputs = [
              pkgs.meson
              pkgs.ninja
              pkgs.pkg-config
            ];

            buildInputs = [
              hyprland.packages.${system}.hyprland.dev
              pkgs.pixman
              pkgs.libdrm
            ] ++ hyprland.packages.${system}.hyprland.buildInputs;
          };
        }
      );

      defaultPackage = eachSystem (system: self.packages.${system}.semmety);

      devShells = eachSystem (
        system:
        let
          pkgs = pkgsFor.${system};
        in
        {
          default = pkgs.mkShell.override { stdenv = pkgs.gcc15Stdenv; } {
            shellHook = ''
              meson setup build --reconfigure
            '';

            inputsFrom = [
              self.packages.${system}.semmety
            ];

            packages = with pkgs; [
              clang-tools
            ];

            CPATH = pkgs.lib.concatStringsSep ":" [
              "${pkgs.gcc15Stdenv.cc.cc}/include/c++/${pkgs.gcc15Stdenv.cc.cc.version}"
              "${pkgs.gcc15Stdenv.cc.cc}/include/c++/${pkgs.gcc15Stdenv.cc.cc.version}/x86_64-unknown-linux-gnu"
              "${pkgs.gcc15Stdenv.cc.libc.dev}/include"
            ];
          };
        }
      );
    };
}
