#!/usr/bin/env bash
# Everything that can be checked without a board, a display or a provider.
#
#   ./tests/check.sh
#
# Adding a suite is a directory under tests/ with a run.sh, plus one word in
# the list below.
#
# Deliberately no `set -e`: one failing suite must not stop the others, or a
# single break hides every other result. Output is filtered to summary lines,
# and the exit code is the verdict.

set -uo pipefail
cd "$(dirname "$0")/.."

fail=0

stage() { printf '\n=== %s ===\n' "$1"; }

report() {
    if [ "$1" -eq 0 ]; then
        printf '  ok\n'
    else
        printf '  FAILED\n'
        fail=1
    fi
}

# Qt's test binaries need the platform plugins, which each suite's run.sh
# locates for itself -- see tests/ui/run.sh for why that is not a one-liner.

stage "tuning (payload, provider seam, transport)"
if [ -f tests/tuning/tuning.pro ]; then
    (
        cd tests/tuning || exit 1
        # Build from clean: a failed build otherwise leaves the previous binary
        # for the run step to test, which reads as a pass.
        make clean >/dev/null 2>&1
        qmake tuning.pro >/dev/null 2>&1 && make -j8 >/dev/null 2>&1 || exit 1
        timeout 120 ./tst_tuning 2>&1 | grep -E "^Totals" || exit 1
    )
    report $?
else
    printf '  skipped: tests/tuning not present\n'
fi

stage "ui (widget structure, parameters, interaction)"
if [ -f tests/ui/ui.pro ]; then
    (
        cd tests/ui || exit 1
        make clean >/dev/null 2>&1
        qmake ui.pro >/dev/null 2>&1 && make -j8 >/dev/null 2>&1 || exit 1
        # run.sh enforces its own timeout, so a hang is a failure not a pass.
        ./run.sh 2>&1 | grep -E "^(FAIL|Totals)" || exit 1
    )
    report $?
else
    printf '  skipped: tests/ui not present\n'
fi

stage "cli (flags, the offline dump, the screenshot path)"
if [ -x tests/cli/run.sh ]; then
    # Needs the application built, not a test binary of its own: these are
    # paths through main.cpp, exercised as a process.
    (cd tests/cli && ./run.sh 2>&1 | tail -3) || exit 1
    report $?
else
    printf '  skipped: tests/cli not present\n'
fi

stage "branding (no upstream product name in display strings)"
(
    # A widget test cannot see all of these: the welcome heading lives in
    # pagewelcome.ui, and that page is QML-backed and not in the suite, so
    # reverting it passed every test. A grep over the sources can see it.
    #
    # Two patterns are forbidden. Any "VESC(R) Tool" spelling is always a
    # product-name claim, and "VESC Tool" as a quoted string literal is this
    # program naming itself. Licence headers say `part of VESC Tool.` without
    # quotes, so they are not matched.
    #
    # appstyle.cpp is the one documented exception: setApplicationName is not
    # displayed, it is what QSettings resolves paths from, and changing it
    # would strand existing users' settings. See ATTRIBUTION.md.
    # Two files may name it, and both are deliberate:
    #   utility.cpp  -- the about box says "a fork of VESC(R) Tool", which is a
    #                   nominative reference and the honest thing to state.
    #   tests/       -- this rule's own description and the test that checks it.
    hits=$(grep -rnE 'VESC(®|&#174;|&reg;) Tool' \
             --include='*.cpp' --include='*.h' --include='*.ui' \
             --include='*.qml' --include='*.xml' \
             . 2>/dev/null \
           | grep -vE '/maddy/|/qmarkdowntextedit/|/QCodeEditor/|/build/|/obj/' \
           | grep -vE '^\./utility\.cpp:|^\./tests/' || true)

    # Any occurrence of the bare name, not just an exact "VESC Tool" literal:
    # the first version of this rule missed
    # tr("You have not finished the VESC Tool introduction...") because the
    # name was in the middle of a longer string. The GPL headers every file
    # inherited are excluded by their own wording.
    lits=$(grep -rn 'VESC Tool' \
             --include='*.cpp' --include='*.h' --include='*.ui' \
             --include='*.qml' --include='*.xml' \
             . 2>/dev/null \
           | grep -vE '/maddy/|/qmarkdowntextedit/|/QCodeEditor/|/build/|/obj/' \
           | grep -vE 'part of VESC Tool|VESC Tool is free software|VESC Tool is distributed' \
           | grep -vE '^\./appstyle\.cpp:|^\./utility\.cpp:|^\./tests/' || true)

    if [ -n "$hits$lits" ]; then
        printf '%s\n' "$hits" "$lits" | sed '/^$/d' | sed 's/^/  /'
        exit 1
    fi
)
report $?

printf '\n'
if [ $fail -eq 0 ]; then
    printf 'all suites passed\n'
else
    printf 'FAILURES above\n'
fi

exit $fail
