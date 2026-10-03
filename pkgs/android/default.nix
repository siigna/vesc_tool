# The Android SDK, NDK and JDK this fork builds against.
#
# Everything here comes from nixpkgs and is pinned. Qt for Android does not:
# nixpkgs packages host Qt only (5.15.19 and 6.x), with no Android
# cross-compilation, so Qt is provisioned separately -- see
# tests/android/README.md and the qt-android script in the dev shell.
#
# Versions are chosen by what Qt 5.15 supports, not by what is newest:
#
#   NDK 21.4.7075529 (r21e)
#       doc.qt.io/qt-5/android-getting-started.html gives "NDK r20b or r21"
#       for Qt 5.14 and later. A newer NDK removes the GCC-era headers and
#       the toolchain layout Qt 5.15's mkspecs expect.
#   platform 31
#       Qt 5.15's supported range is API 21 to 31
#       (doc.qt.io/qt-5/android.html). It is also what the one live,
#       actively-updated Qt 5.15 app in F-Droid compiles against.
#   build-tools 30.0.3
#       NOT 31.0.0, which matches the platform. Build-tools 31 removed `dx`
#       in favour of `d8`, and AGP 4.2.2 validates a build-tools install by
#       looking for `dx` -- so 31.0.0 fails the whole package step with
#
#           Installed Build Tools revision 31.0.0 is corrupted.
#           Remove and install again using the SDK Manager.
#
#       which is a lie: the install is complete, AGP just wants a tool that
#       Google deleted. 30.0.3 is the last version that ships `dx`, and
#       compileSdkVersion still comes from the platform, so this costs
#       nothing. A newer AGP would not need `dx`, but a newer AGP needs a
#       newer Gradle than Qt 5.15's templates tolerate.
#   JDK 11
#       "As of Qt 5.15.8, JDK 11 or later is supported for Qt for Android."
#       JDK 8 is what upstream's build_android used, and is too old for the
#       Gradle this needs; a much newer JDK breaks the Gradle Qt 5.15 ships.
{
  pkgs,
}:
let
  composed = pkgs.androidenv.composeAndroidPackages {
    cmdLineToolsVersion = "13.0";
    platformToolsVersion = "35.0.2";
    buildToolsVersions = [ "30.0.3" ];
    platformVersions = [ "31" ];

    includeNDK = true;
    ndkVersions = [ "21.4.7075529" ];

    # Not wanted, and each one is a large download.
    includeEmulator = false;
    includeSystemImages = false;
    includeSources = false;
    includeCmake = false;
  };
  # A second set, with the emulator and a system image.
  #
  # Separate from the one above on purpose: the system image is well over a
  # gigabyte, and neither a normal build nor CI has any use for it. Only
  # devShells.emulator pulls this in.
  #
  # API 31 default x86_64, which matches VT_ANDROID_TARGET_SDK exactly, so
  # what runs is what the package declares it targets. x86_64 because the
  # host is, and because KVM makes that the only configuration fast enough to
  # be worth doing -- an arm64 image under full emulation takes minutes to
  # reach a home screen.
  #
  # "default" rather than google_apis: the Google APIs image adds Play
  # services this application never touches, and the AOSP image still carries
  # DocumentsUI, which is what the storage access framework picker is.
  composedEmulator = pkgs.androidenv.composeAndroidPackages {
    cmdLineToolsVersion = "13.0";
    platformToolsVersion = "35.0.2";
    buildToolsVersions = [ "30.0.3" ];
    platformVersions = [ "31" ];

    includeNDK = true;
    ndkVersions = [ "21.4.7075529" ];

    includeEmulator = true;
    includeSystemImages = true;
    systemImageTypes = [ "default" ];
    abiVersions = [ "x86_64" ];

    includeSources = false;
    includeCmake = false;
  };
in
rec {
  inherit composed composedEmulator;

  emulatorSdk = composedEmulator.androidsdk;
  emulatorSdkRoot = "${emulatorSdk}/libexec/android-sdk";

  sdk = composed.androidsdk;

  # Where the SDK actually lands inside the derivation. androiddeployqt and
  # gradle both want this as a path, not as a package.
  sdkRoot = "${sdk}/libexec/android-sdk";
  ndkRoot = "${sdkRoot}/ndk/21.4.7075529";

  jdk = pkgs.jdk11_headless;

  # The Qt version fetched by the qt-android script. Pinned here so the shell,
  # the build script and CI cannot disagree about it.
  #
  # 5.15.2 is the last open-source release the official installer serves for
  # Android. nixpkgs' host Qt is 5.15.19 (the KDE patch collection), which is
  # fine for the desktop build but has no Android kit to offer.
  qtVersion = "5.15.2";

  # aqtinstall's architecture string for an Android target differs by Qt major
  # version: "android" for Qt >= 5.14 < 6.0, "android_armv7" for Qt >= 6.0.
  # Passing the Qt 6 spelling to 5.15.2 fails with "The packages ['qt_base']
  # were not found while parsing XML of package information!", which is the
  # most common way this goes wrong.
  qtArch = "android";

  # What the emulator runs. The Qt kit fetched for the device ABIs already
  # contains this one -- the 5.15 Android download is multi-ABI, carrying
  # armeabi-v7a, arm64-v8a, x86 and x86_64 -- so testing on an emulator costs
  # no second Qt download.
  emulatorAbi = "x86_64";
  emulatorImage = "system-images;android-31;default;x86_64";

  # Deliberately empty.
  #
  # For Qt 5.15.2's Android target, everything this application links is in
  # the base install -- qtbase, qtdeclarative, qtquickcontrols (which is
  # where QtQuick.Extras lives), qtquickcontrols2, qtgraphicaleffects, qtsvg,
  # qtconnectivity, qtpositioning, qtandroidextras, qttools and
  # qtimageformats. `aqt list-qt linux android --modules 5.15.2 android`
  # offers only add-ons this fork does not use:
  #
  #   qtcharts qtdatavis3d qtlottie qtnetworkauth qtpurchasing qtquick3d
  #   qtquicktimeline qtscript
  #
  # Passing the base modules to -m fails the whole install with "The packages
  # [...] were not found while parsing XML of package information!", which
  # reads like a network or arch problem and is not.
  qtModules = [ ];
}
