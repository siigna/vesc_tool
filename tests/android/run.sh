#!/usr/bin/env bash
# Checks the Android build configuration without an SDK, an NDK or a device.
#
#   ./run.sh
#
# Nothing in this fork builds for Android, and nothing ever has in CI, so the
# whole Android configuration was unverified: a path in DISTFILES that no
# longer exists, an undefined qmake variable in the manifest template, a
# manifest that does not parse, or a declared service class with no Java file
# behind it are all invisible until someone sets up a full Android toolchain.
#
# These are the checks that need none of that. What they cannot do is prove
# the thing compiles -- see README.md for what an actual build job would cost.

set -uo pipefail
cd "$(dirname "$0")/../.."

fail=0

ok()  { printf '  ok      %s\n' "$1"; }
bad() { printf '  FAILED  %s\n' "$1"; fail=1; }

# ---------------------------------------------------------------- DISTFILES

# Every file the project claims to ship with the Android package. qmake treats
# DISTFILES as informational, so a stale entry is never an error at build time
# -- which is how android/gradlew.bat sat in that list, absent from the tree
# and from upstream's, for as long as the list has existed.
#
# android/AndroidManifest.xml is the one legitimate absence: QMAKE_SUBSTITUTES
# generates it from the .in at qmake time. Its template is checked instead.
missing=""
while read -r f; do
    [ -z "$f" ] && continue

    if [ "$f" = "android/AndroidManifest.xml" ]; then
        if [ -f "android/AndroidManifest.xml.in" ]; then
            continue
        fi
        missing="$missing $f.in"
        continue
    fi

    [ -e "$f" ] || missing="$missing $f"
done < <(sed -n '/^DISTFILES += \\/,/^$/p' vesc_tool.pro \
         | grep -oE 'android/[^ \\]+')

if [ -n "$missing" ]; then
    bad "DISTFILES names files that do not exist:$missing"
else
    ok "every android/ path in DISTFILES exists"
fi

# ANDROID_PACKAGE_SOURCE_DIR is what androiddeployqt copies wholesale.
if [ -d android ]; then
    ok "ANDROID_PACKAGE_SOURCE_DIR exists"
else
    bad "android/ does not exist, but ANDROID_PACKAGE_SOURCE_DIR points at it"
fi

# ------------------------------------------------------- manifest template

python3 tests/android/manifest.py
[ $? -ne 0 ] && fail=1

# ------------------------------------------------------------- built package

# The manifest is not the artefact. This is the only stage that can see what
# the APK actually declares, and it is the stage that would have caught an
# APK declaring minSdkVersion 1 while every manifest check reported 23.
#
# Skipped rather than failed when there is no APK, because check.sh is meant
# to run with no SDK at all. The skip says what to run.
apk=$(ls -t build/android/escargot-*.apk 2>/dev/null | head -1)
aapt2=""

if [ -n "${ANDROID_SDK_ROOT:-}" ]; then
    aapt2="$ANDROID_SDK_ROOT/build-tools/${VT_ANDROID_BUILD_TOOLS:-30.0.3}/aapt2"
fi

if [ -z "$apk" ]; then
    printf '  skipped  no APK in build/android; run tests/android/build.sh\n'
elif [ -z "$aapt2" ] || [ ! -x "$aapt2" ]; then
    printf '  skipped  no aapt2; run inside: nix develop .#android\n'
else
    printf '  %s\n' "$(basename "$apk")"
    python3 tests/android/apk.py "$apk" "$aapt2"
    [ $? -ne 0 ] && fail=1
fi

exit $fail
