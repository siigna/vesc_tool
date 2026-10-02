#!/usr/bin/env bash
# Runs the widget suite. No board, no display server, no network.
#
#   ./run.sh                 everything
#   ./run.sh structure       one slot (any QTest filter works)
#
# Plugin discovery is shared with the other suites -- see tests/qtenv.sh.

set -uo pipefail
cd "$(dirname "$0")"

. "$(dirname "$0")/../qtenv.sh"
qt_env_setup || exit 2

# The harness pins everything else itself, before the QApplication exists.
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

if [ ! -x ./tst_ui ]; then
    echo "run.sh: ./tst_ui is not built. qmake && make -j8" >&2
    exit 2
fi

# Stale mismatch files from a previous run must not survive into this one --
# update-baselines.sh promotes whatever is in actual/, and once swept a stale
# file turns a broken page into the baseline.
rm -rf actual

# A hang must not read as a pass.
timeout 300 ./tst_ui "$@"
status=$?

# The light palette is a separate run: the colour tables for the two themes are
# independent lists in appstyle.cpp, and icon lookups resolve to a different
# directory, so a missing light-theme icon or colour is invisible to the dark
# run. Skipped when the caller asked for specific tests.
if [ $status -eq 0 ] && [ $# -eq 0 ]; then
    timeout 300 ./tst_ui --light
    status=$?
fi

if [ $status -eq 124 ]; then
    echo "FAILED: timed out after 300s" >&2
fi

exit $status
