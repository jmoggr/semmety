# somewhat inspired by:
# https://github.com/shezdy/hyprsplit/blob/main/flake.nix
{
  description = "Semmety";

  inputs = {
    hyprland = {
      url = "github:hyprwm/Hyprland/b496e2c71817aae5560af04b8c6439c39f4e05d8";
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
          semmety = pkgs.stdenv.mkDerivation {
            pname = "semmety";
            version = "0.1";

            src = ./.;

            nativeBuildInputs = [
              pkgs.meson
              pkgs.ninja
              pkgs.pkg-config
              pkgs.gcc14
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
          default = pkgs.mkShell {
            shellHook = ''
              meson setup build --reconfigure
            '';
            inputsFrom = [ self.packages.${system}.semmety ];
          };
        }
      );
    };
}
