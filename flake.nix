{
  description = "intrinsic";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      for_each_system = f:
        nixpkgs.lib.genAttrs systems
          (system: f system (import nixpkgs { inherit system; }));

      mk_intrinsic = pkgs:
        pkgs.llvmPackages.stdenv.mkDerivation {
          pname = "intrinsic";
          version = "rolling";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
          ];

          buildInputs = with pkgs; [
            ncurses
            sqlite
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_TESTING=ON"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            cmake --build . --target intrinsic_tests
            ctest --output-on-failure
            runHook postCheck
          '';

          meta = with pkgs.lib; {
            mainProgram = "intrinsic";
            platforms = platforms.linux ++ platforms.darwin;
          };
        };

      mk_dev_shell = pkgs:
        pkgs.mkShell {
          packages = with pkgs;
            [
              just
              cmake
              ninja
              pkg-config
              clang
              lldb
              ncurses
              sqlite
              cppcheck
            ]
            ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
              llvmPackages.compiler-rt
            ];

          shellHook = ''
            export CC=clang
            export CXX=clang++

            if [ -n "''${BASH_VERSION-}" ]; then
              export PS1="[nix-shell:intrinsic] \u@\h:\w\$ "
            elif [ -n "''${ZSH_VERSION-}" ]; then
              export PROMPT="[nix-shell:intrinsic] %n@%m:%~%# "
            fi
          '';
        };
    in
    {
      devShells = for_each_system (system: pkgs: {
        default = mk_dev_shell pkgs;
      });

      packages = for_each_system (system: pkgs: {
        default = mk_intrinsic pkgs;
      });

      apps = for_each_system (system: pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/intrinsic";
        };
      });
    };
}


