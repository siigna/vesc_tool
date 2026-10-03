#!/usr/bin/env bash
# Builds an Android APK. Replaces upstream's build_android, which hardcoded
# one developer's machine.
#
#   nix develop .#android --command tests/android/build.sh mobile
#   nix develop .#android --command tests/android/build.sh full
#   nix develop .#android --command tests/android/build.sh mobile arm64-v8a
#
# Variant is `mobile` (the QML phone UI) or `full` (the desktop widget UI).
# They are separate applications with separate ids, so both can be installed
# at once. The ABI list defaults to both that Qt 5.15 supports on ARM, built
# into one universal APK: F-Droid cannot install an app bundle, so a single
# APK carrying every ABI is the only shape that works there.
#
# What this does NOT get from nix: Qt for Android. nixpkgs packages host Qt
# only, so Qt is fetched by aqtinstall into a cache directory the first time,
# at the version the dev shell pins. That download is the one non-reproducible
# input; it is why an F-Droid recipe has to build Qt from source instead.

set -uo pipefail

cd "$(dirname "$0")/../.."
root=$(pwd)

variant="${1:-mobile}"
abis="${2:-armeabi-v7a arm64-v8a}"

case "$variant" in
    mobile|full) ;;
    *) echo "build.sh: variant must be 'mobile' or 'full', got '$variant'" >&2
       exit 2 ;;
esac

# VT_ANDROID_QT_MODULES is deliberately not in this list: it is legitimately
# empty for Qt 5.15, and an emptiness check would reject the correct value.
for v in ANDROID_SDK_ROOT ANDROID_NDK_ROOT JAVA_HOME \
         VT_ANDROID_QT_VERSION VT_ANDROID_QT_ARCH \
         VT_ANDROID_PLATFORM; do
    if [ -z "${!v:-}" ]; then
        echo "build.sh: $v is not set. Run inside: nix develop .#android" >&2
        exit 2
    fi
done

say() { printf '\n=== %s ===\n' "$1"; }

# ----------------------------------------------------------------- Qt

qt_cache="${VT_ANDROID_QT_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/escargot/qt-android}"
qt_root="$qt_cache/$VT_ANDROID_QT_VERSION/$VT_ANDROID_QT_VERSION/android"

if [ ! -x "$qt_root/bin/qmake" ]; then
    say "fetching Qt $VT_ANDROID_QT_VERSION for Android"
    echo "  into $qt_cache"
    echo "  (once; delete that directory to force a refetch)"

    mkdir -p "$qt_cache/$VT_ANDROID_QT_VERSION" || exit 1

    # -m is passed only when there is something to pass. For Qt 5.15's
    # Android target the list is empty, because every module this application
    # uses is in the base install; naming them anyway fails the whole install.
    mods=()
    if [ -n "${VT_ANDROID_QT_MODULES:-}" ] && [ -n "${VT_ANDROID_QT_MODULES// /}" ]; then
        # shellcheck disable=SC2086
        mods=(-m $VT_ANDROID_QT_MODULES)
    fi

    aqt install-qt \
        linux android "$VT_ANDROID_QT_VERSION" "$VT_ANDROID_QT_ARCH" \
        "${mods[@]}" \
        -O "$qt_cache/$VT_ANDROID_QT_VERSION" || {
            echo "build.sh: aqt failed." >&2
            echo "  If it reports \"packages ['qt_base'] were not found\", the" >&2
            echo "  arch string is wrong for this Qt: 5.15 wants 'android'," >&2
            echo "  Qt 6 wants 'android_armv7'. See pkgs/android/default.nix." >&2
            exit 1
        }
fi

if [ ! -f "$qt_root/bin/qmake" ]; then
    echo "build.sh: no qmake at $qt_root/bin/qmake after fetching" >&2
    exit 1
fi

# ------------------------------------------------------- patch the host tools

# The kit is built for a generic Linux: its host tools are x86-64 ELF linked
# against /lib64/ld-linux-x86-64.so.2, which does not exist here. Running one
# reports "Could not start dynamically linked executable", which reads like a
# corrupt download and is not. So the interpreter and library path are
# rewritten once, in place, against the glibc the dev shell pins.
#
# Only bin/ is touched. lib/ holds the Android libraries, which are ARM ELF
# and must not be patched -- they are the build's output, not its tools.
stamp="$qt_root/.escargot-patched"

if [ ! -f "$stamp" ]; then
    say "making the Qt host tools runnable"

    if [ -z "${VT_ANDROID_HOST_INTERP:-}" ]; then
        echo "build.sh: VT_ANDROID_HOST_INTERP is not set" >&2
        exit 2
    fi

    patched=0
    for f in "$qt_root"/bin/*; do
        [ -f "$f" ] || continue

        # Only x86-64 ELF. bin/ also holds perl scripts (fixqt4headers.pl),
        # shell wrappers and qt.conf, and patchelf on those is an error.
        #
        # Matched in file's own order: "ELF 64-bit LSB executable, x86-64,
        # ... interpreter /lib64/ld-linux-x86-64.so.2". An earlier version of
        # this glob expected "executable" after "x86-64", matched nothing, and
        # reported patching zero tools -- which the check below caught.
        case "$(file -b "$f")" in
            *"ELF 64-bit"*"x86-64"*) ;;
            *) continue ;;
        esac

        patchelf --set-interpreter "$VT_ANDROID_HOST_INTERP" \
                 --set-rpath "$VT_ANDROID_HOST_LIBS" "$f" 2>/dev/null \
            && patched=$((patched + 1))
    done

    if [ "$patched" -eq 0 ]; then
        echo "build.sh: patched no host tools; expected qmake at least" >&2
        exit 1
    fi

    echo "  rewrote $patched host tool(s)"
    touch "$stamp"
fi

# Proves the patching worked, rather than finding out sixty lines later with a
# message that looks like something else.
if ! "$qt_root/bin/qmake" -query QT_VERSION >/dev/null 2>&1; then
    echo "build.sh: $qt_root/bin/qmake still will not run." >&2
    echo "  Try: rm -rf $qt_root/.escargot-patched and re-run, or" >&2
    echo "  rm -rf $qt_cache to refetch." >&2
    "$qt_root/bin/qmake" -query QT_VERSION 2>&1 | sed 's/^/  | /' >&2
    exit 1
fi

echo "Qt:   $qt_root ($("$qt_root/bin/qmake" -query QT_VERSION))"

# ----------------------------------------------------------- writable SDK

# The SDK in the nix store is read-only and gradle writes into it, so it is
# copied once into a cache directory. A symlink tree is not enough: gradle
# creates files inside platforms/ and licenses/.
sdk="${VT_ANDROID_SDK_RW:-${XDG_CACHE_HOME:-$HOME/.cache}/escargot/android-sdk}"

if [ ! -d "$sdk/platforms" ]; then
    say "copying the SDK somewhere writable"
    echo "  $ANDROID_SDK_ROOT -> $sdk"
    rm -rf "$sdk"
    mkdir -p "$(dirname "$sdk")"
    cp -r --no-preserve=mode,ownership "$ANDROID_SDK_ROOT" "$sdk" || exit 1
fi

export ANDROID_SDK_ROOT="$sdk"
export ANDROID_HOME="$sdk"

echo "SDK:  $ANDROID_SDK_ROOT"
echo "NDK:  $ANDROID_NDK_ROOT"
echo "JDK:  $JAVA_HOME"
export PATH="$JAVA_HOME/bin:$qt_root/bin:$PATH"

# ------------------------------------------------------------------ build

# A shadow build per variant, so the two do not overwrite each other's
# object files or deployment settings. Upstream's script rm -rf'd between
# passes, which meant a failed second pass left the first one's APK looking
# like its output.
build="$root/build/android-$variant"

config="release_android"
if [ "$variant" = "mobile" ]; then
    config="$config build_mobile"
fi

say "qmake ($variant, $abis)"
rm -rf "$build"
mkdir -p "$build" || exit 1
cd "$build" || exit 1

"$qt_root/bin/qmake" "$root/vesc_tool.pro" \
    -spec android-clang \
    -config release \
    "CONFIG += $config" \
    ANDROID_ABIS="$abis" || exit 1

say "make"
make -j"$(nproc 2>/dev/null || echo 4)" || exit 1

say "make install"
rm -rf "$build/pkg"
make install INSTALL_ROOT="$build/pkg" || exit 1

# qmake writes this next to the .pro it was given, not into the build dir.
settings="$build/android-vesc_tool-deployment-settings.json"
if [ ! -f "$settings" ]; then
    settings="$root/android-vesc_tool-deployment-settings.json"
fi

if [ ! -f "$settings" ]; then
    echo "build.sh: no deployment settings json; looked in $build and $root" >&2
    exit 1
fi

say "androiddeployqt"

# --release makes an unsigned release APK rather than the debug-signed one
# upstream's script harvested out of outputs/apk/debug/. Signing is a separate
# step so an unsigned artifact is never mistaken for a signed one.
deploy_args=(
    --input "$settings"
    --output "$build/pkg"
    --android-platform "$VT_ANDROID_PLATFORM"
    --gradle
    --release
)

if [ -n "${VT_ANDROID_KEYSTORE:-}" ]; then
    if [ -z "${VT_ANDROID_KEYSTORE_ALIAS:-}" ]; then
        echo "build.sh: VT_ANDROID_KEYSTORE set without VT_ANDROID_KEYSTORE_ALIAS" >&2
        exit 2
    fi

    # Passwords come from the environment and are passed by androiddeployqt to
    # jarsigner; they are never written into the tree. --storepass is read from
    # VT_ANDROID_KEYSTORE_PASS rather than taken on the command line, so it
    # does not land in a process listing.
    deploy_args+=(
        --sign "$VT_ANDROID_KEYSTORE" "$VT_ANDROID_KEYSTORE_ALIAS"
    )
    export QT_ANDROID_KEYSTORE_PASS="${VT_ANDROID_KEYSTORE_PASS:-}"
    export QT_ANDROID_KEY_PASS="${VT_ANDROID_KEY_PASS:-$QT_ANDROID_KEYSTORE_PASS}"
    echo "  signing with $VT_ANDROID_KEYSTORE (alias $VT_ANDROID_KEYSTORE_ALIAS)"
else
    echo "  unsigned: set VT_ANDROID_KEYSTORE and VT_ANDROID_KEYSTORE_ALIAS to sign"
fi

"$qt_root/bin/androiddeployqt" "${deploy_args[@]}" || exit 1

# ----------------------------------------------------------------- result

apk=$(find "$build/pkg/build/outputs/apk" -name '*.apk' -print 2>/dev/null | head -1)

if [ -z "$apk" ]; then
    echo "build.sh: androiddeployqt reported success but produced no apk" >&2
    exit 1
fi

out="$root/build/android/escargot-$variant-$(sed -n 's/^VT_VERSION = //p' "$root/app.pri" | tr -d ' ').apk"
mkdir -p "$(dirname "$out")"
cp "$apk" "$out" || exit 1

say "built"
echo "  $out"
ls -lh "$out" | awk '{print "  " $5}'

exit 0
