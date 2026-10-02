{
  description = "Packages VESC Tool into a flake.";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";
    flake-utils.url = "github:numtide/flake-utils";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    bldc-fw = {
      url = "github:vedderb/bldc/master";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  # TODO: Add support for building on/for other systems.
  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      treefmt-nix,
      bldc-fw,
    }@inputs:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
        treefmtEval = treefmt-nix.lib.evalModule pkgs ./treefmt.nix;
        selfPkgs = import ./pkgs {
          inherit pkgs;
          bldc-fw = bldc-fw.packages.${system}.bldc-fw;
          src = self;
        };
      in
      {
        packages = selfPkgs // {
          default = selfPkgs.vesc-tool;
        };

        # `nix develop`, which is also what tests/check.sh is run inside.
        #
        # Without this, `nix develop` fell back to the default package's build
        # environment, which has the Qt modules but nothing else -- and in
        # particular no xvfb-run, so the six pages that need an OpenGL context
        # silently skipped on every run. run.sh reports that skip, but a skip
        # is easy to read past.
        devShells.default = pkgs.mkShell {
          inputsFrom = [ selfPkgs.vesc-tool ];

          packages = with pkgs; [
            # tests/ui/run.sh runs the GL tier under a real X server with
            # Mesa's software rasteriser, because the offscreen platform
            # reports no GL capability at all.
            xvfb-run
            mesa
            libGL

            # tests/mutate.py
            python3
          ];
        };

        # For `nix fmt`
        formatter = treefmtEval.config.build.wrapper;

        checks = {
          # For `nix flake check`
          formatting = treefmtEval.config.build.check self;
        };
      }
    )
    // {
      overlays.default = import ./overlay.nix {
        inherit bldc-fw;
        src = self;
      };
      # For development in the nix repl
      inherit self;
    };
}
