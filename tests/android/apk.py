#!/usr/bin/env python3
"""Asserts what a built APK actually declares.

    tests/android/apk.py <apk> [aapt2]

This exists because the manifest is not the artefact. tests/android/manifest.py
checks the template and what qmake writes, and both said minSdkVersion 23 --
while the first APK that built declared minSdkVersion 1, because AGP replaces
the manifest uses-sdk with its defaultConfig and an unset
defaultConfig.minSdkVersion defaults to 1. Nothing but `aapt2 dump badging`
on the package itself could see that.

The permission set is imported from manifest.py rather than restated, so the
two checks cannot drift apart.
"""

import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manifest  # noqa: E402  (after the path fix, deliberately)

EXPECTED_MIN_SDK = 23
EXPECTED_TARGET_SDK = 31
PACKAGE_PREFIX = "io.github.siigna.escargot"

# Both ABIs, unless told otherwise.
#
# VT_ANDROID_EXPECT_ABIS exists for the per-push CI build, which compiles one
# ABI because two take twice as long and the point of that job is to catch
# link errors. The alternative was to skip this whole check on those runs,
# which would have given up the package id, SDK level, permission and label
# assertions as well -- for the sake of one line.
EXPECTED_ABIS = set(
    (os.environ.get("VT_ANDROID_EXPECT_ABIS")
     or "arm64-v8a armeabi-v7a").replace(",", " ").split())

fail = False


def ok(msg):
    print("  ok      %s" % msg)


def bad(msg):
    global fail
    print("  FAILED  %s" % msg)
    fail = True


def badging(apk, aapt2):
    try:
        out = subprocess.run([aapt2, "dump", "badging", apk],
                             capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as exc:
        bad("aapt2 could not read %s: %s" % (apk, exc.stderr.strip()[:200]))
        return None

    return out.stdout


def check(text):
    m = re.search(r"^package: name='([^']+)' versionCode='([^']+)' "
                  r"versionName='([^']+)'", text, re.M)
    if not m:
        bad("no package line in the badging output")
        return

    package, code, name = m.group(1), m.group(2), m.group(3)

    if package.startswith(PACKAGE_PREFIX):
        ok("package %s, version %s (code %s)" % (package, name, code))
    else:
        bad("package is %s; expected something under %s"
            % (package, PACKAGE_PREFIX))

    try:
        if int(code) < 100:
            bad("versionCode %s is implausibly low; it is derived from "
                "VT_VERSION and should have at least three digits" % code)
    except ValueError:
        bad("versionCode %r is not an integer" % code)

    # The two that only the package can answer.
    for label, pattern, want in (
            ("minSdkVersion", r"^sdkVersion:'(\d+)'", EXPECTED_MIN_SDK),
            ("targetSdkVersion", r"^targetSdkVersion:'(\d+)'",
             EXPECTED_TARGET_SDK)):
        m = re.search(pattern, text, re.M)
        if not m:
            bad("the package declares no %s" % label)
            continue

        got = int(m.group(1))
        if got != want:
            extra = ""
            if label == "minSdkVersion" and got == 1:
                extra = (" -- that is AGP's default, so defaultConfig in "
                         "android/build.gradle is not reading qtMinSdkVersion")
            bad("%s is %d, expected %d%s" % (label, got, want, extra))
        else:
            ok("%s is %d" % (label, got))

    got = set(re.findall(r"uses-permission: name='([^']+)'", text))
    expected = set(manifest.EXPECTED_PERMISSIONS)

    added = sorted(got - expected)
    lost = sorted(expected - got)

    if added:
        extra = ""
        if "android.permission.WRITE_EXTERNAL_STORAGE" in added:
            extra = (" -- WRITE_EXTERNAL_STORAGE back in the package means "
                     "the INSERT_PERMISSIONS placeholder has returned to the "
                     "manifest template")
        bad("the package requests permissions the manifest does not: %s%s"
            % (", ".join(added), extra))
    if lost:
        bad("the package is missing: %s" % ", ".join(lost))
    if not added and not lost:
        ok("permission set matches the manifest exactly (%d)" % len(got))

    m = re.search(r"^native-code: (.+)$", text, re.M)
    if not m:
        bad("the package declares no native code at all")
    else:
        abis = set(re.findall(r"'([^']+)'", m.group(1)))
        if abis != EXPECTED_ABIS:
            bad("ABIs are %s, expected %s"
                % (", ".join(sorted(abis)), ", ".join(sorted(EXPECTED_ABIS))))
        else:
            ok("one package carrying %s" % ", ".join(sorted(abis)))

    m = re.search(r"^application-label:'([^']*)'", text, re.M)
    if not m or not m.group(1):
        bad("the package has no application label")
    elif "VESC" in m.group(1):
        bad("the application label is %r, which is upstream's product name"
            % m.group(1))
    else:
        ok("application label is %r" % m.group(1))


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip())
        return 2

    apk = argv[1]
    aapt2 = argv[2] if len(argv) > 2 else "aapt2"

    if not os.path.isfile(apk):
        bad("no such apk: %s" % apk)
        return 1

    text = badging(apk, aapt2)
    if text is None:
        return 1

    check(text)
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
