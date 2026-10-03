#!/usr/bin/env python3
"""Checks AndroidManifest.xml.in without an Android toolchain.

Run through tests/android/run.sh, from the repository root.

Why this emulates qmake
-----------------------
The template is not valid XML, deliberately. Every value is wrapped in the
`'"..."'` idiom, the XML declaration included:

    <?xml version='"1.0"'?>
    <manifest package='"$${VT_ANDROID_PACKAGE}"' ...>

QMAKE_SUBSTITUTES strips the single quotes and leaves the double ones, so what
qmake writes to android/AndroidManifest.xml is `version="1.0"` and
`package="io.github.siigna.escargot"`. Write the values with plain double
quotes instead and the output is `version=1.0`, which is not XML at all.

An earlier version of this file parsed the *template* as XML, concluded the
manifest was unusable, and asserted that it stay that way. That was wrong: it
was reading the input of a substitution as though it were the output. The
check now applies the same two transformations qmake applies -- variable
substitution, then removal of the single quotes -- and asserts on the result.

That is an emulation, so it is stated as one. The real generated manifest is
asserted against a built APK by `aapt2 dump badging` once there is a build to
run; see tests/android/README.md.
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

TEMPLATE = "android/AndroidManifest.xml.in"
PROJECT_FILES = ("vesc_tool.pro", "app.pri")
JAVA_ROOT = "android/src"
ANDROID_NS = "http://schemas.android.com/apk/res/android"
A = "{%s}" % ANDROID_NS

# The permission set, as a decision rather than an accident. Each one has a
# call site; tests/android/README.md records what and why, and the four that
# used to be here and are not any more.
EXPECTED_PERMISSIONS = {
    "android.permission.BLUETOOTH",
    "android.permission.BLUETOOTH_SCAN",
    "android.permission.BLUETOOTH_CONNECT",
    "android.permission.ACCESS_COARSE_LOCATION",
    "android.permission.ACCESS_FINE_LOCATION",
    "android.permission.ACCESS_BACKGROUND_LOCATION",
    "android.permission.FOREGROUND_SERVICE",
    "android.permission.FOREGROUND_SERVICE_LOCATION",
    "android.permission.POST_NOTIFICATIONS",
    "android.permission.WAKE_LOCK",
}

fail = False


def ok(msg):
    print("  ok      %s" % msg)


def bad(msg):
    global fail
    print("  FAILED  %s" % msg)
    fail = True


def qmake_variables(src):
    """The qmake variables the template uses, and their values."""
    wanted = set(re.findall(r"\$\$\{?([A-Za-z_][A-Za-z0-9_]*)\}?", src))

    assignments = {}
    for name in PROJECT_FILES:
        with open(name) as fh:
            for var, value in re.findall(
                    r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$", fh.read(), re.M):
                assignments.setdefault(var, value.strip())

    undefined = sorted(wanted - set(assignments))
    if undefined:
        bad("manifest uses qmake variables nothing defines: %s"
            % ", ".join(undefined))
    else:
        ok("every qmake variable in the manifest is defined (%s)"
           % ", ".join(sorted(wanted)))

    return wanted, assignments


def substitute(src, wanted, assignments):
    """What qmake would write, near enough to assert on.

    Values are not resolved recursively -- VT_ANDROID_VERSION is a
    $$replace() call on VT_VERSION, which only qmake can evaluate -- so each
    variable becomes a placeholder of the right shape instead: a number where
    the real value is numeric, so the integer checks below mean something.
    """
    numeric = {"VT_ANDROID_VERSION", "VT_ANDROID_MIN_SDK",
               "VT_ANDROID_TARGET_SDK"}

    out = src
    for var in wanted:
        if var in numeric:
            # Three digits, because that is the shape the derived version code
            # has to keep: VT_VERSION 7.02 becomes 702.
            value = "702" if var == "VT_ANDROID_VERSION" else assignments[var]
        else:
            value = assignments[var]

        out = re.sub(r"\$\$\{?%s\}?" % re.escape(var), value, out)

    # The transformation that makes the template valid.
    return out.replace("'", "")


def check(root, assignments):
    if root.tag != "manifest":
        bad("root element is %s, not manifest" % root.tag)
        return

    # The namespace is the thing most easily broken by editing the quoting,
    # and the breakage is invisible: every android: attribute silently stops
    # resolving while the file still parses.
    for el in root.iter():
        for key in el.attrib:
            if key.startswith("{") and not key.startswith(A):
                bad("attribute %s on <%s> is not in the Android namespace; "
                    "check xmlns:android" % (key, el.tag))
                return

    ok("xmlns:android resolves to %s" % ANDROID_NS)

    code = root.get(A + "versionCode")
    try:
        if len(str(int(code))) < 3:
            bad("versionCode %r has fewer than three digits; the derived code "
                "goes backwards if VT_VERSION loses a decimal place" % code)
        else:
            ok("versionCode is an integer of at least three digits (%s)" % code)
    except (TypeError, ValueError):
        bad("versionCode %r is not an integer" % code)

    package = root.get("package")
    if package and package.startswith("io.github.siigna.escargot"):
        ok("package is this fork's own id (%s)" % package)
    else:
        bad("package is %r; a published fork needs its own application id"
            % package)

    sdk = root.find("uses-sdk")
    if sdk is None:
        bad("no uses-sdk element")
    else:
        target = int(sdk.get(A + "targetSdkVersion"))
        minimum = int(sdk.get(A + "minSdkVersion"))

        # Qt 5.15 supports API 21 to 31; above that nothing has validated the
        # Java bindings or the Gradle that Qt ships.
        if target > 31:
            bad("targetSdkVersion %d is above Qt 5.15's supported ceiling of "
                "31" % target)
        elif minimum < 23:
            bad("minSdkVersion %d is below 23, which Android refuses to "
                "install" % minimum)
        else:
            ok("minSdk %d / targetSdk %d, inside Qt 5.15's supported range"
               % (minimum, target))

    got = {p.get(A + "name") for p in root.findall("uses-permission")}

    added = sorted(got - EXPECTED_PERMISSIONS)
    lost = sorted(EXPECTED_PERMISSIONS - got)

    if added or lost:
        if added:
            bad("manifest requests permissions this check does not expect: %s"
                % ", ".join(added))
        if lost:
            bad("manifest no longer requests: %s" % ", ".join(lost))
    else:
        ok("permission set is exactly the %d expected" % len(got))

    app = root.find("application")
    if app is None:
        bad("no application element")
        return

    if A + "requestLegacyExternalStorage" in app.attrib:
        bad("requestLegacyExternalStorage is back; it is ignored from API 30 "
            "and the storage permissions it went with are gone")

    service = app.find("service")
    if service is None:
        bad("no service element; the foreground logging service is how "
            "background GNSS works")
        return

    if service.get(A + "exported") != "false":
        bad("the foreground service is exported; it is started only from this "
            "app's own QML, so exporting it lets any app start our location "
            "service")
    else:
        ok("the foreground service is not exported")

    if service.get(A + "foregroundServiceType") != "location":
        bad("the service has no location foregroundServiceType, which API 34+ "
            "requires")

    return service.get(A + "name")


def java_sources_exist(root, declared_name):
    packages = []
    for dirpath, _, files in os.walk(JAVA_ROOT):
        if any(f.endswith(".java") for f in files):
            packages.append(os.path.relpath(dirpath, JAVA_ROOT)
                            .replace(os.sep, "."))

    names = []
    for tag in ("activity", "service", "receiver", "provider"):
        for el in root.iter(tag):
            name = el.get(A + "name")
            if name:
                names.append((tag, name))

    checked = 0
    for tag, name in names:
        # Qt's own org.qtproject.* classes come from the Qt libraries.
        if not any(name.startswith(pkg + ".") for pkg in packages):
            continue

        checked += 1
        path = os.path.join(JAVA_ROOT, name.replace(".", os.sep) + ".java")

        if not os.path.isfile(path):
            bad("manifest declares <%s> %s, but %s does not exist"
                % (tag, name, path))

    if checked == 0:
        bad("no manifest component resolved to this project's Java sources "
            "(packages found: %s; declared: %s)"
            % (", ".join(packages) or "none",
               ", ".join(n for _, n in names) or "none"))
    else:
        ok("all %d declared component(s) in %s have Java sources"
           % (checked, ", ".join(sorted(packages))))


def main():
    with open(TEMPLATE) as fh:
        src = fh.read()

    # A bare double hyphen anywhere inside an XML comment is a parse error,
    # and it is the easiest thing to introduce while writing a comment.
    for body in re.findall(r"<!--(.*?)-->", src, re.S):
        if "--" in body:
            bad("an XML comment contains a double hyphen, which is a parse "
                "error: %r" % body.strip()[:60])
            break

    wanted, assignments = qmake_variables(src)

    if fail:
        return 1

    filled = substitute(src, wanted, assignments)

    try:
        root = ET.fromstring(filled)
    except ET.ParseError as exc:
        bad("what qmake would write does not parse: %s" % exc)
        return 1

    ok("substitutes into well-formed XML")

    check(root, assignments)
    java_sources_exist(root, None)

    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
