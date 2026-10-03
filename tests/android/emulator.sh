#!/usr/bin/env bash
# Runs the app on an emulator and reports what Android says about it.
#
#   nix develop .#emulator --command tests/android/emulator.sh
#   nix develop .#emulator --command tests/android/emulator.sh --keep
#
# This is the first thing in the tree that RUNS the application rather than
# building it. Everything before it -- the manifest checks, the badging
# assertions, the QML suite -- reads inputs or artefacts. None of it can say
# whether the app starts.
#
# What it can and cannot cover:
#
#   covered      launching, the QML loading, the C++ and JNI actually
#                resolving at runtime, permissions as the system sees them,
#                and anything visible in logcat
#   not covered  Bluetooth. The emulator has no BLE radio, and nothing
#                emulates one. Finding a controller over BLE needs hardware
#                and always will.
#
# The APK is built for x86_64 because the emulator is, and because KVM makes
# that the only configuration fast enough to be useful. The Qt kit already
# carries that ABI, so it costs no extra download.

set -uo pipefail
cd "$(dirname "$0")/../.."
root=$(pwd)

keep=0
[ "${1:-}" = "--keep" ] && keep=1

for v in ANDROID_SDK_ROOT VT_ANDROID_EMULATOR_ABI VT_ANDROID_EMULATOR_IMAGE; do
    if [ -z "${!v:-}" ]; then
        echo "emulator.sh: $v is not set. Run inside: nix develop .#emulator" >&2
        exit 2
    fi
done

if [ ! -e /dev/kvm ]; then
    echo "emulator.sh: no /dev/kvm. Without it the emulator is too slow to" >&2
    echo "  be worth waiting for; this refuses rather than appearing to hang." >&2
    exit 2
fi

abi="$VT_ANDROID_EMULATOR_ABI"
pkg="io.github.siigna.escargot"
avd="escargot-test"
port=5584            # Away from the default 5554, so a desktop emulator is untouched.
serial="emulator-$port"

say()  { printf '\n=== %s ===\n' "$1"; }
ok()   { printf '  ok      %s\n' "$1"; }
bad()  { printf '  FAILED  %s\n' "$1"; fail=1; }
fail=0

tmp=$(mktemp -d) || exit 1

cleanup() {
    if [ -n "${emu_pid:-}" ] && kill -0 "$emu_pid" 2>/dev/null; then
        if [ "$keep" -eq 1 ]; then
            printf '\nemulator left running as %s (pid %s)\n' "$serial" "$emu_pid"
            printf 'stop it with: adb -s %s emu kill\n' "$serial"
            return
        fi
        adb -s "$serial" emu kill >/dev/null 2>&1
        sleep 2
        kill "$emu_pid" 2>/dev/null
    fi
    rm -rf "$tmp"
}
trap cleanup EXIT

# ------------------------------------------------------------------ the APK

say "building for $abi"

# A separate build directory from the device builds, so this cannot consume
# or clobber an arm64 package.
VT_ANDROID_VARIANT_DIR="android-emu" \
VT_ANDROID_OUT_SUFFIX="-$abi" \
    "$root/tests/android/build.sh" mobile "$abi" > "$tmp/build.log" 2>&1
st=$?

if [ "$st" -ne 0 ]; then
    bad "build failed; last lines:"
    tail -15 "$tmp/build.log" | sed 's/^/  | /'
    exit 1
fi

apk="$root/build/android/escargot-mobile-$abi-$(sed -n 's/^VT_VERSION = //p' "$root/app.pri" | tr -d ' ').apk"
[ -f "$apk" ] || apk=""

if [ -z "$apk" ]; then
    bad "no APK after a successful build"
    exit 1
fi

ok "built $(basename "$apk") ($(du -h "$apk" | cut -f1))"

# --------------------------------------------------------- a throwaway key

# Android refuses an unsigned package outright:
#
#     INSTALL_PARSE_FAILED_NO_CERTIFICATES: Failed collecting certificates
#
# build.sh produces unsigned packages on purpose, so this signs a copy with a
# key generated here, used once, and thrown away with the temporary directory.
#
# Deliberately NOT the release key from tests/android/keystore.sh. That one
# signs what goes on a phone, and a test harness should not need it, touch
# it, or be able to. The password below is a constant precisely because the
# key is worthless: it exists for the length of this run.

key="$tmp/emulator-test.keystore"
signed="$tmp/signed.apk"

keytool -genkeypair -keystore "$key" -storetype PKCS12 \
    -storepass emulator -keypass emulator -alias test \
    -keyalg RSA -keysize 2048 -validity 1 \
    -dname "CN=escargot emulator test, OU=throwaway" \
    > "$tmp/keytool.log" 2>&1 || {
        bad "could not create the throwaway signing key:"
        tail -5 "$tmp/keytool.log" | sed 's/^/  | /'
        exit 1
    }

apksigner sign --ks "$key" --ks-pass pass:emulator --key-pass pass:emulator \
    --ks-key-alias test --out "$signed" "$apk" \
    > "$tmp/apksigner.log" 2>&1 || {
        bad "apksigner failed:"
        tail -10 "$tmp/apksigner.log" | sed 's/^/  | /'
        exit 1
    }

# Proves the signature is there, rather than assuming apksigner's exit code.
if apksigner verify --print-certs "$signed" > "$tmp/verify.log" 2>&1; then
    ok "signed with a throwaway key ($(grep -m1 -oE 'CN=[^,]*' "$tmp/verify.log" || echo 'CN unknown'))"
else
    bad "the signed APK does not verify:"
    tail -5 "$tmp/verify.log" | sed 's/^/  | /'
    exit 1
fi

apk="$signed"

# ------------------------------------------------------------------- the AVD

say "emulator"

# Recreated every run. An AVD carries its own userdata, so a leftover one
# would mean testing a first launch against an app that had already run, and
# the storage access framework grant in particular is per-install state.
rm -rf "$tmp/avd"
export ANDROID_AVD_HOME="$tmp/avd"
export ANDROID_SDK_HOME="$tmp"
mkdir -p "$ANDROID_AVD_HOME" || exit 1

# avdmanager runs on its own JDK. cmdline-tools 13.0 compiled it for Java 17
# and JDK 11 will not load it; the build needs JDK 11, because that is what
# Qt 5.15's gradle wants. So JAVA_HOME is overridden for this one command
# rather than for the shell.
echo no | JAVA_HOME="${VT_ANDROID_AVD_JAVA_HOME:-$JAVA_HOME}" avdmanager create avd \
    -n "$avd" -k "$VT_ANDROID_EMULATOR_IMAGE" \
    --abi "$abi" --force > "$tmp/avd.log" 2>&1 || {
        bad "avdmanager failed:"
        tail -10 "$tmp/avd.log" | sed 's/^/  | /'
        exit 1
    }

ok "created AVD $avd from $VT_ANDROID_EMULATOR_IMAGE"

# -no-window because there is no display here and none is needed; the point
# is what the app reports, not what it looks like. swiftshader_indirect gives
# a working GL implementation in software, which Qt Quick needs -- the app
# aborts without one, exactly as the offscreen QPA platform does on the
# desktop.
emulator -avd "$avd" -no-window -no-audio -no-boot-anim -no-snapshot \
    -gpu swiftshader_indirect -port "$port" \
    -wipe-data > "$tmp/emulator.log" 2>&1 &
emu_pid=$!

adb start-server >/dev/null 2>&1

printf '  waiting for boot'
booted=0
for _ in $(seq 1 180); do
    if ! kill -0 "$emu_pid" 2>/dev/null; then
        printf '\n'
        bad "the emulator exited while booting:"
        tail -15 "$tmp/emulator.log" | sed 's/^/  | /'
        exit 1
    fi

    state=$(adb -s "$serial" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r\n')
    if [ "$state" = "1" ]; then
        booted=1
        break
    fi
    printf '.'
    sleep 2
done
printf '\n'

if [ "$booted" -ne 1 ]; then
    bad "emulator did not finish booting in 360s"
    tail -15 "$tmp/emulator.log" | sed 's/^/  | /'
    exit 1
fi

ok "booted: $(adb -s "$serial" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r\n')" \
   "(API $(adb -s "$serial" shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r\n'))"

# ----------------------------------------------------------------- install

say "install"

adb -s "$serial" install -r "$apk" > "$tmp/install.log" 2>&1
if ! grep -q Success "$tmp/install.log"; then
    bad "install failed:"
    tail -10 "$tmp/install.log" | sed 's/^/  | /'
    exit 1
fi

ok "installed $pkg"

# What the system itself thinks the package is, which is a stronger statement
# than aapt reading the file.
sdk=$(adb -s "$serial" shell dumpsys package "$pkg" 2>/dev/null \
      | grep -oE "targetSdk=[0-9]+" | head -1)
ver=$(adb -s "$serial" shell dumpsys package "$pkg" 2>/dev/null \
      | grep -oE "versionCode=[0-9]+" | head -1)
ok "the system reports $ver $sdk"

# ------------------------------------------------------------------- launch

say "launch"

adb -s "$serial" logcat -c >/dev/null 2>&1
adb -s "$serial" shell monkey -p "$pkg" -c android.intent.category.LAUNCHER 1 \
    > /dev/null 2>&1

# Qt loads its libraries, starts a QML engine and builds the scene; that is
# slower than the activity appearing.
sleep 20

adb -s "$serial" logcat -d > "$tmp/logcat.txt" 2>/dev/null

if adb -s "$serial" shell pidof "$pkg" >/dev/null 2>&1; then
    ok "still running after 20s"
else
    bad "not running 20s after launch"
fi

# ------------------------------------------------------------------ verdict

say "what Android said"

crash=$(grep -cE "FATAL EXCEPTION|beginning of crash|signal [0-9]+ \(SIG" "$tmp/logcat.txt")
if [ "$crash" -gt 0 ]; then
    bad "$crash crash marker(s) in logcat:"
    grep -E "FATAL EXCEPTION|beginning of crash|signal [0-9]+ \(SIG" -A6 \
        "$tmp/logcat.txt" | head -30 | sed 's/^/  | /'
else
    ok "no crash markers"
fi

# A QML error leaves the scene partly or wholly unbuilt, and the app keeps
# running -- which is exactly the failure tests/qml exists to catch on the
# desktop. Here it catches the same thing with the real Qt libraries, the
# real resource bundle and the real JNI.
qml=$(grep -E "qrc:/|\.qml:[0-9]+" "$tmp/logcat.txt" \
      | grep -vE "^\s*$" | grep -icE "error|is not installed|cannot|undefined")
if [ "$qml" -gt 0 ]; then
    bad "$qml QML error line(s):"
    grep -E "qrc:/|\.qml:[0-9]+" "$tmp/logcat.txt" \
        | grep -iE "error|is not installed|cannot|undefined" | head -15 \
        | sed 's/^/  | /'
else
    ok "no QML errors"
fi

# Qt's own complaints, which do not necessarily stop the app.
for pat in "Could not find the Qt platform plugin" \
           "library .* not found" \
           "UnsatisfiedLinkError" \
           "dlopen failed"; do
    n=$(grep -cE "$pat" "$tmp/logcat.txt")
    [ "$n" -gt 0 ] && bad "logcat: $pat ($n)"
done

printf '\n'
if [ "$fail" -eq 0 ]; then
    printf 'the application runs\n'
else
    printf 'FAILURES above\n'
    cp "$tmp/logcat.txt" "$root/build/android/emulator-logcat.txt" 2>/dev/null \
        && printf 'full logcat: build/android/emulator-logcat.txt\n'
fi

exit $fail
