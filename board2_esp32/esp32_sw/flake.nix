{
  description = "Development environment for ESP32 projects with PlatformIO";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            platformio
            arduino
            git
          ];

          shellHook = ''
            echo "Welcome to the ESP32 development environment!"
            echo "PlatformIO is available. Use 'pio' to access PlatformIO commands."
          '';
        };
      }
    );
}
