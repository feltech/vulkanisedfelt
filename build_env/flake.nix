{
  description = "A Nix-flake-based C/C++ development environment";
  nixConfig.bash-prompt-suffix = "(vulkanisedfelt) ";

  #inputs.nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" ];
      forEachSupportedSystem = f: nixpkgs.lib.genAttrs supportedSystems (system: f {
        pkgs = import nixpkgs { inherit system; config.allowUnfree = true; };
      });
    in
    {
      devShells = forEachSupportedSystem ({ pkgs }: {
        default = pkgs.mkShell.override
          {
            # Override stdenv in order to change compiler:
            # stdenv = pkgs.clangStdenv;
          }
          {

            packages = with pkgs; [            
              cmake
              conan
              ninja
              vulkan-loader
              vulkan-headers
              vulkan-validation-layers
              vulkan-utility-libraries
              # For conan */system packages. Hint: nix-locate --whole-name dependency_name.pc | grep -v "^("
              pkg-config
              # For Conan egl/system
              libGL
              # For Conan xkeyboard-config/system
              xkeyboard_config
              # For Conan xorg/system
              xorg.libX11
              xorg.libfontenc
              xorg.libICE
              xorg.libSM
              xorg.libXau
              xorg.libXaw
              xorg.libXcomposite
              xorg.libXcursor
              xorg.libXdamage
              xorg.libXdmcp
              xorg.libXext
              xorg.libXfixes
              xorg.libXi
              xorg.libXinerama
              xorg.libXinerama
              xorg.libXmu
              xorg.libXpm
              xorg.libXrandr
              xorg.libXrender
              xorg.libXres
              xorg.libXScrnSaver
              xorg.libXt
              xorg.libXtst
              xorg.libXv
              xorg.libXxf86vm
              xorg.libxcb
              xorg.libxkbfile
              xorg.xcbutil
              xorg.xcbutilwm
              xorg.xcbutilimage
              xorg.xcbutilkeysyms
              xorg.xcbutilrenderutil
              xcb-util-cursor
              libuuid # Also provided by util-linux
            ];

            env = {
              # Vulkan loads driver libraries at runtime.
              LD_LIBRARY_PATH = "/run/opengl-driver/lib";
            };
          };
      });
    };
}