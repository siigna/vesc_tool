#!/usr/bin/env bash
# Everything that can be checked without a board, a display or a provider.
#
#   ./tests/check.sh
#   ./tests/check.sh --build-app     also build the application first
#
# The cli suite tests paths through main.cpp as a process, so it needs the
# application itself, not a test binary. That build takes about three and a
# half minutes against this suite's forty-five seconds, so it is not the
# default -- but in a fresh clone the cli stage has nothing to run, which is
# what --build-app is for, and what CI uses.
#
# Adding a suite is a directory under tests/ with a run.sh, plus one word in
# the list below.
#
# Deliberately no `set -e`: one failing suite must not stop the others, or a
# single break hides every other result. Output is filtered to summary lines,
# and the exit code is the verdict.

set -uo pipefail
cd "$(dirname "$0")/.."

build_app=0

for arg in "$@"; do
    case "$arg" in
        --build-app) build_app=1 ;;
        *) printf 'check.sh: unknown argument %s\n' "$arg" >&2; exit 2 ;;
    esac
done

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

# Each stage filters its suite's output down to summary lines, which is what
# makes this readable -- and what makes a crash unreadable. A tier that dies
# before printing a summary matched nothing, so the stage printed a bare
# FAILED and threw away the only evidence. That is how a GL-tier crash in CI
# looked identical to every other kind of failure.
#
# So the raw output is kept, and shown on failure only.
raw=$(mktemp -d)
trap 'rm -rf "$raw"' EXIT

explain() {
    if [ "$1" -ne 0 ] && [ -s "$2" ]; then
        printf '  --- last %d lines of raw output ---\n' 20
        tail -20 "$2" | sed 's/^/  | /'
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
        timeout 120 ./tst_tuning > "$raw/tuning" 2>&1
        st=$?
        grep -E "^Totals" "$raw/tuning"
        exit $st
    )
    st=$?
    explain $st "$raw/tuning"
    report $st
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
        ./run.sh > "$raw/ui" 2>&1
        st=$?
        grep -E "^(FAIL|SKIP|Totals)" "$raw/ui"
        exit $st
    )
    st=$?
    explain $st "$raw/ui"
    report $st
else
    printf '  skipped: tests/ui not present\n'
fi

stage "qml (mobile components load, instantiate and warn about nothing)"
if [ -f tests/qml/qml.pro ]; then
    (
        cd tests/qml || exit 1
        # No `make clean` here, unlike the ui stage. That clean is there
        # because a failed build leaves the previous binary for the run step,
        # which reads as a pass -- and this suite has a second guard against
        # exactly that: run.sh refuses to run a tst_qml older than any file in
        # mobile/, since the QML is linked in as a resource. Cleaning as well
        # would mean recompiling the whole application twice per check.sh.
        qmake qml.pro >/dev/null 2>&1 && make -j8 >/dev/null 2>&1 || exit 1
        ./run.sh > "$raw/qml" 2>&1
        st=$?
        grep -E "^(FAIL|SKIP|Totals)" "$raw/qml"
        exit $st
    )
    st=$?
    explain $st "$raw/qml"
    report $st
else
    printf '  skipped: tests/qml not present\n'
fi

if [ "$build_app" -eq 1 ]; then
    stage "application (for the cli suite, which runs it as a process)"
    (
        qmake -config release \
            "CONFIG += release_lin build_original exclude_fw" >/dev/null \
            && make -j"$(nproc 2>/dev/null || echo 8)" >/dev/null 2>&1
    )
    report $?
fi

stage "cli (flags, the offline dump, the screenshot path)"
if [ -x tests/cli/run.sh ]; then
    # Needs the application built, not a test binary of its own: these are
    # paths through main.cpp, exercised as a process.
    #
    # No `|| exit 1` here. The other stages use that inside their subshell, to
    # end the subshell; out here it ended check.sh, so a fresh clone -- where
    # the application is not built and this suite cannot run -- never reached
    # the branding stage at all, and the file's own promise that one failing
    # suite does not stop the others was false.
    (cd tests/cli && ./run.sh > "$raw/cli" 2>&1; st=$?; tail -3 "$raw/cli"; exit $st)
    st=$?
    explain $st "$raw/cli"
    report $st
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
    #
    # The file types matter as much as the patterns. This started at .cpp,
    # .h, .ui, .qml and .xml, and three user-visible strings were outside all
    # five: the macOS and iOS bundle name in vesc_tool.pro, the Android
    # launcher label in AndroidManifest.xml.in -- which is the name under the
    # icon and in the task switcher -- and the title of the notification the
    # foreground logging service shows, in a .java file. So .pro, .pri, .in,
    # .gradle and .java are included now.
    #
    # The `(\./)?` in the exclusions is not decoration. GNU grep prefixes a
    # recursive match with `./`; ugrep, which some shells alias grep to, does
    # not. Anchored on `^\./` alone, every exclusion silently stopped
    # matching outside the dev shell and the three documented exceptions
    # failed the stage.
    #
    # Still deliberately outside: the Java package and Android application id
    # com.vedder.vesc. That is an identity rather than a display string, and
    # changing it makes the build a different app that cannot upgrade an
    # installed one.
    # Two files may name it, and both are deliberate:
    #   utility.cpp  -- the about box says "a fork of VESC(R) Tool", which is a
    #                   nominative reference and the honest thing to state.
    #   tests/       -- this rule's own description and the test that checks it.
    hits=$(grep -rnE 'VESC(®|&#174;|&reg;) Tool' \
             --include='*.cpp' --include='*.h' --include='*.ui' \
             --include='*.qml' --include='*.xml' --include='*.pro' \
             --include='*.pri' --include='*.in' --include='*.gradle' \
             --include='*.java' \
             . 2>/dev/null \
           | grep -vE '/maddy/|/qmarkdowntextedit/|/QCodeEditor/|/build/|/obj/' \
           | grep -vE '^(\./)?utility\.cpp:|^(\./)?tests/' || true)

    # Any occurrence of the bare name, not just an exact "VESC Tool" literal:
    # the first version of this rule missed
    # tr("You have not finished the VESC Tool introduction...") because the
    # name was in the middle of a longer string. The GPL headers every file
    # inherited are excluded by their own wording.
    lits=$(grep -rn 'VESC Tool' \
             --include='*.cpp' --include='*.h' --include='*.ui' \
             --include='*.qml' --include='*.xml' --include='*.pro' \
             --include='*.pri' --include='*.in' --include='*.gradle' \
             --include='*.java' \
             . 2>/dev/null \
           | grep -vE '/maddy/|/qmarkdowntextedit/|/QCodeEditor/|/build/|/obj/' \
           | grep -vE 'part of VESC Tool|VESC Tool is free software|VESC Tool is distributed' \
           | grep -vE '^(\./)?appstyle\.cpp:|^(\./)?utility\.cpp:|^(\./)?tests/' || true)

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
