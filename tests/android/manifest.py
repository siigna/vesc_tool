#!/usr/bin/env python3
"""Checks AndroidManifest.xml.in without an Android toolchain.

Run through tests/android/run.sh, from the repository root.

The template is substituted by qmake, so an undefined variable becomes an
empty attribute rather than an error -- and an empty android:versionCode is a
manifest the Play Store rejects, which is a long way downstream of here.

On the double quoting
--------------------
Every value in the template is wrapped in a second pair of quotes:

    <?xml version='"1.0"'?>
    <manifest xmlns:android='"http://schemas.android.com/apk/res/android"'
              android:versionCode='"$${VT_ANDROID_VERSION}"' ...>

That is upstream's own spelling, unchanged in this fork, and it is not
cosmetic:

  * The XML declaration is ill-formed. A VersionNum cannot contain a quote.
  * Every attribute value carries the quotes into its value, so
    android:versionCode is the string `"1"` rather than the integer 1.
  * The xmlns:android URI is itself quoted, so none of the android:
    attributes are in the Android namespace -- which is why asking the parsed
    tree for android:versionCode returns nothing at all.

So this is not a usable manifest as committed, and whatever produces
upstream's releases is not this file as it stands. The history around it says
as much: "Another attempt at checking in the correct file...".

It is not rewritten here. Seventy-nine attributes cannot be re-quoted and
called correct without an Android build to try it against, and this fork has
none. What is asserted instead is that the file stays *uniform* -- all values
quoted, which is the state recorded here, or none, which is the fixed state.
A mixed file means a hand edit that did not decide which, and that is the
thing worth catching.
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

TEMPLATE = "android/AndroidManifest.xml.in"
PROJECT_FILES = ("vesc_tool.pro", "app.pri")
JAVA_ROOT = "android/src"

fail = False


def ok(msg):
    print("  ok      %s" % msg)


def bad(msg):
    global fail
    print("  FAILED  %s" % msg)
    fail = True


def qmake_variables_are_defined(src):
    # Both spellings the template uses: $$NAME and $${NAME}.
    wanted = set(re.findall(r"\$\$\{?([A-Za-z_][A-Za-z0-9_]*)\}?", src))

    defined = set()
    for name in PROJECT_FILES:
        with open(name) as fh:
            defined.update(re.findall(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=",
                                      fh.read(), re.M))

    undefined = sorted(wanted - defined)

    if undefined:
        bad("manifest uses qmake variables nothing defines: %s"
            % ", ".join(undefined))
    else:
        ok("every qmake variable in the manifest is defined (%s)"
           % ", ".join(sorted(wanted)))


def parse(src):
    """The tree, with the declaration normalised -- the only way to get one."""
    filled = re.sub(r"\$\$\{?[A-Za-z_][A-Za-z0-9_]*\}?", "1", src)
    norm = filled.replace("<?xml version='\"1.0\"'?>",
                          '<?xml version="1.0"?>', 1)

    try:
        root = ET.fromstring(norm)
    except ET.ParseError as exc:
        bad("manifest template does not parse: %s" % exc)
        return None

    ok("manifest template parses as XML once the declaration is normalised")
    return root


def quoting_is_uniform(root):
    values = [v for el in root.iter() for v in el.attrib.values()]
    quoted = [v for v in values
              if len(v) >= 2 and v.startswith('"') and v.endswith('"')]

    if len(quoted) == len(values):
        ok("manifest is uniformly double-quoted (%d/%d values), as recorded"
           % (len(quoted), len(values)))
    elif not quoted:
        bad("manifest is no longer double-quoted at all. If that was a "
            "deliberate fix, record it here and drop this check -- but "
            "confirm it against a real Android build first")
    else:
        bad("manifest is double-quoted in %d of %d values. Mixed means a hand "
            "edit that did not decide which; see the note at the top of this "
            "file" % (len(quoted), len(values)))


def android_name(el):
    """The android:name of an element, by local name.

    Matched this way rather than through the namespace, because the quoted
    xmlns means nothing resolves through the Android namespace.
    """
    for key, value in el.attrib.items():
        if key.endswith("name") and "android" in key:
            return value.strip('"')

    return ""


def declared_components_have_sources(root):
    declared = []
    for tag in ("activity", "service", "receiver", "provider"):
        for el in root.iter(tag):
            name = android_name(el)
            if name:
                declared.append((tag, name))

    packages = []
    for dirpath, _, files in os.walk(JAVA_ROOT):
        if any(f.endswith(".java") for f in files):
            packages.append(os.path.relpath(dirpath, JAVA_ROOT)
                            .replace(os.sep, "."))

    checked = 0
    for tag, name in declared:
        # Qt's own org.qtproject.* classes come from the Qt libraries, so only
        # this project's packages are checked.
        if not any(name.startswith(pkg + ".") for pkg in packages):
            continue

        checked += 1
        path = os.path.join(JAVA_ROOT, name.replace(".", os.sep) + ".java")

        if not os.path.isfile(path):
            bad("manifest declares <%s> %s, but %s does not exist"
                % (tag, name, path))

    # Finding none at all means the match broke, not that everything is fine.
    if checked == 0:
        bad("no manifest component resolved to this project's Java sources; "
            "either the packages moved or the match broke")
    else:
        ok("all %d declared component(s) in %s have Java sources"
           % (checked, ", ".join(sorted(packages))))


def main():
    with open(TEMPLATE) as fh:
        src = fh.read()

    qmake_variables_are_defined(src)

    root = parse(src)
    if root is not None:
        quoting_is_uniform(root)
        declared_components_have_sources(root)

    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
