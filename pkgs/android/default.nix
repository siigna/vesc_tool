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
#   build-tools 31.0.0
#       Kept with the platform rather than newest, because aapt2's manifest
#       handling is the thing most likely to differ.
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
    buildToolsVersions = [ "31.0.0" ];
    platformVersions = [ "31" ];

    includeNDK = true;
    ndkVersions = [ "21.4.7075529" ];

    # Not wanted, and each one is a large download.
    includeEmulator = false;
    includeSystemImages = false;
    includeSources = false;
    includeCmake = false;
  };
in
rec {
  inherit composed;

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

  qtModules = [
    "qtbase"
    "qtdeclarative"
    "qtquickcontrols"      # QtQuick.Extras, which both gauge components use
    "qtquickcontrols2"
    "qtgraphicaleffects"
    "qtsvg"
    "qtconnectivity"       # Bluetooth
    "qtpositioning"        # GNSS
    "qtandroidextras"
    "qttools"
    "qtimageformats"
  ];
}
