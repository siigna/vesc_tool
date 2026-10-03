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

        # A second instance, for the Android toolchain only.
        #
        # The Android SDK is unfree and nixpkgs refuses to build it without an
        # explicit acceptance. That acceptance is scoped to this instance
        # rather than set on the one above, so the desktop build and the test
        # suites stay on a package set with no licence exceptions at all.
        pkgsAndroid = import nixpkgs {
          inherit system;
          config = {
            android_sdk.accept_license = true;

            # Scoped to this instance, which exists only to hold the Android
            # toolchain. Google's SDK licence is why its components are
            # unfree, and they do not share a name prefix to match on --
            # `platform-tools`, `build-tools` and `ndk` are bare names -- so
            # the boundary is the package set rather than a predicate.
            #
            # The desktop build and every test suite use the instance above,
            # which has no licence exceptions at all. Anything added to this
            # shell inherits the exception, so add only toolchain here.
            allowUnfree = true;
          };
        };

        android = import ./pkgs/android { pkgs = pkgsAndroid; };
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
            # Xvfb itself, not just the xvfb-run wrapper: tests/ui/run.sh
            # starts the server by hand so that it can wait for -displayfd,
            # which is what makes the GL tier reliable under load.
            xvfb
            # The probe that decides when the X server is actually usable.
            xdpyinfo
            mesa
            libGL

            # tests/mutate.py
            python3
          ];
        };

        # Everything an Android build needs except Qt, which nixpkgs has no
        # Android cross-compilation for; `qt-android` fetches that.
        #
        #   nix develop .#android --command tests/android/build.sh mobile
        devShells.android = pkgsAndroid.mkShell {
          packages = [
            android.sdk
            android.jdk
            pkgsAndroid.aqtinstall
            pkgsAndroid.p7zip
            # The Qt kit aqt downloads is built for a generic Linux, so its
            # host tools cannot run here at all until their interpreter is
            # rewritten. See build.sh.
            pkgsAndroid.patchelf
            pkgsAndroid.file
            # androiddeployqt shells out to these.
            pkgsAndroid.which
            pkgsAndroid.unzip
            pkgsAndroid.git
          ];

          # androiddeployqt and Qt's mkspecs read these by name. ANDROID_NDK_ROOT
          # and ANDROID_SDK_ROOT are what Qt 5.15 looks for; ANDROID_HOME is
          # what gradle looks for, and they are deliberately the same paths.
          ANDROID_SDK_ROOT = android.sdkRoot;
          ANDROID_HOME = android.sdkRoot;
          ANDROID_NDK_ROOT = android.ndkRoot;
          ANDROID_NDK_HOME = android.ndkRoot;
          JAVA_HOME = "${android.jdk}";

          # What build.sh rewrites the fetched Qt host tools to use.
          #
          # qmake, moc, rcc, uic and androiddeployqt in the Qt kit are x86-64
          # ELF executables linked against /lib64/ld-linux-x86-64.so.2, which
          # does not exist on NixOS. Running one reports "Could not start
          # dynamically linked executable", which looks like a corrupt
          # download and is not.
          VT_ANDROID_HOST_INTERP =
            "${pkgsAndroid.glibc}/lib/ld-linux-x86-64.so.2";
          VT_ANDROID_HOST_LIBS = pkgsAndroid.lib.makeLibraryPath [
            pkgsAndroid.glibc
            pkgsAndroid.stdenv.cc.cc.lib
            pkgsAndroid.zlib
          ];

          # Read by tests/android/build.sh, so the shell is the single place
          # these are pinned.
          VT_ANDROID_QT_VERSION = android.qtVersion;
          VT_ANDROID_QT_ARCH = android.qtArch;
          VT_ANDROID_QT_MODULES = builtins.concatStringsSep " " android.qtModules;
          VT_ANDROID_PLATFORM = "android-31";

          # build.sh needs this to find aapt2 inside the SDK, so it is pinned
          # here with everything else rather than written twice.
          VT_ANDROID_BUILD_TOOLS = "30.0.3";

          shellHook = ''
            # The SDK in the nix store is read-only, and gradle wants to write
            # into it. androiddeployqt is pointed at a writable copy instead;
            # build.sh makes it.
            export VT_ANDROID_SDK_RO="${android.sdkRoot}"
          '';
        };

        # The same toolchain plus an emulator and a system image, for running
        # the thing rather than only building it.
        #
        #   nix develop .#emulator --command tests/android/emulator.sh
        devShells.emulator = pkgsAndroid.mkShell {
          packages = [
            android.emulatorSdk
            android.jdk
            # A second JDK, for avdmanager only.
            #
            # cmdline-tools 13.0 ships an avdmanager compiled for Java 17
            # (class file 61), and JDK 11 refuses it: "has been compiled by a
            # more recent version of the Java Runtime". The build cannot move
            # to 17 -- Qt 5.15 wants 11, and that is what gradle here runs
            # with -- so the AVD tool gets its own and the build keeps JDK 11.
            pkgsAndroid.jdk17_headless
            pkgsAndroid.aqtinstall
            pkgsAndroid.p7zip
            pkgsAndroid.patchelf
            pkgsAndroid.file
            pkgsAndroid.which
            pkgsAndroid.unzip
            pkgsAndroid.git
          ];

          ANDROID_SDK_ROOT = android.emulatorSdkRoot;
          ANDROID_HOME = android.emulatorSdkRoot;
          ANDROID_NDK_ROOT = "${android.emulatorSdkRoot}/ndk/21.4.7075529";
          ANDROID_NDK_HOME = "${android.emulatorSdkRoot}/ndk/21.4.7075529";
          ANDROID_AVD_HOME = "";
          JAVA_HOME = "${android.jdk}";

          VT_ANDROID_HOST_INTERP =
            "${pkgsAndroid.glibc}/lib/ld-linux-x86-64.so.2";
          VT_ANDROID_HOST_LIBS = pkgsAndroid.lib.makeLibraryPath [
            pkgsAndroid.glibc
            pkgsAndroid.stdenv.cc.cc.lib
            pkgsAndroid.zlib
          ];

          VT_ANDROID_QT_VERSION = android.qtVersion;
          VT_ANDROID_QT_ARCH = android.qtArch;
          VT_ANDROID_QT_MODULES = builtins.concatStringsSep " " android.qtModules;
          VT_ANDROID_PLATFORM = "android-31";
          VT_ANDROID_BUILD_TOOLS = "30.0.3";

          VT_ANDROID_EMULATOR_ABI = android.emulatorAbi;
          VT_ANDROID_EMULATOR_IMAGE = android.emulatorImage;

          # Used for avdmanager and nothing else. JAVA_HOME stays on 11 so
          # the gradle the build runs is unaffected.
          VT_ANDROID_AVD_JAVA_HOME = "${pkgsAndroid.jdk17_headless}";

          shellHook = ''
            export VT_ANDROID_SDK_RO="${android.emulatorSdkRoot}"
          '';
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
