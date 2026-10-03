#!/usr/bin/env python3
"""Looks for upstream's product name in text as a reader sees it.

Run from tests/check.sh, after the two greps there.

Those greps look for the contiguous string "VESC Tool", and that is not how
the name necessarily appears. The intro text in every res/config/*/info.xml
rendered as

    Welcome to VESC Tool. Since this is the first time you start this
    version of ESCargot Tool, the introduction is shown.

while the file said

    Welcome to &lt;span ...&gt;VESC&lt;/span&gt; &lt;span ...&gt;Tool&lt;/span&gt;.

Twenty-six files, one per firmware configuration version, and the grep could
not see any of them because the words are in different tags. It was found by
running the application on an emulator and reading the screen.

So this strips markup, unescapes entities, collapses whitespace, and only
then looks -- which is the same text a user is shown.
"""

import glob
import html
import os
import re
import sys

# Rich text and markup live in these; plain source is covered by the greps in
# check.sh, which also handle its exceptions.
PATTERNS = [
    "res/config/**/*.xml",
    "res/**/*.qml",
    "mobile/*.qml",
    "res/**/*.html",
]

SKIP_DIRS = ("maddy", "qmarkdowntextedit", "QCodeEditor", "build", "obj")

# The GPL header wording every file inherited, which is not a product-name
# claim. Same exceptions the greps in check.sh make.
LICENCE = re.compile(
    r"part of VESC Tool|VESC Tool is free software|VESC Tool is distributed")

# A nominative reference, which is the honest thing to state and is excepted
# in check.sh too.
NOMINATIVE = re.compile(r"fork of VESC(®|&reg;|\(R\))? Tool")

NAME = re.compile(r"VESC(®|\(R\))?\s+Tool")


def rendered(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        raw = fh.read()

    # Entities first: the markup itself is escaped inside XML descriptions,
    # so the tags only become tags after unescaping.
    text = html.unescape(raw)
    text = re.sub(r"<[^>]+>", " ", text)
    text = html.unescape(text)
    return re.sub(r"\s+", " ", text)


def main():
    findings = []

    for pattern in PATTERNS:
        for path in sorted(glob.glob(pattern, recursive=True)):
            if any(("/" + d + "/") in ("/" + path) or path.startswith(d + "/")
                   for d in SKIP_DIRS):
                continue
            if not os.path.isfile(path):
                continue

            text = rendered(path)
            text = LICENCE.sub(" ", text)
            text = NOMINATIVE.sub(" ", text)

            for m in NAME.finditer(text):
                start = max(0, m.start() - 40)
                findings.append((path, text[start:m.end() + 40].strip()))

    if findings:
        print("  upstream's product name, as rendered:")
        for path, ctx in findings[:20]:
            print("    %s" % path)
            print("      ...%s..." % ctx)
        if len(findings) > 20:
            print("    and %d more" % (len(findings) - 20))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
