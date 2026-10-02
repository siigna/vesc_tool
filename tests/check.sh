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

printf '\n'
if [ $fail -eq 0 ]; then
    printf 'all suites passed\n'
else
    printf 'FAILURES above\n'
fi

exit $fail
