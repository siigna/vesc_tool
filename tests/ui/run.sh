#!/usr/bin/env bash
# Runs the widget suite. No board, no display server, no network.
#
#   ./run.sh                 everything
#   ./run.sh structure       one slot (any QTest filter works)
#
# Qt will not start without being told where its platform plugins are. In this
# nix setup QT_PLUGIN_PATH is empty and `qmake -query QT_INSTALL_PLUGINS`
# answers with the -dev output, which has no plugins directory at all: both
# libqoffscreen.so and libqxcb.so live in the separate -bin output. Deriving
# one path from the other keeps this free of store hashes.

set -uo pipefail
cd "$(dirname "$0")"

if [ -z "${QT_PLUGIN_PATH:-}" ]; then
    # The -dev and -bin outputs have different store hashes, so one path cannot
    # be derived from the other by substitution. Match on the Qt version, which
    # qmake does know, and take whichever candidate actually has platforms/.
    ver=$(qmake -query QT_VERSION 2>/dev/null || true)
    dev=$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)

    for cand in "$dev" /nix/store/*-qtbase-"$ver"-bin/lib/qt-"$ver"/plugins; do
        if [ -d "$cand/platforms" ]; then
            export QT_PLUGIN_PATH="$cand"
            break
        fi
    done

    if [ -z "${QT_PLUGIN_PATH:-}" ]; then
        echo "run.sh: cannot find Qt platform plugins for Qt $ver." >&2
        echo "  set QT_PLUGIN_PATH to the directory containing platforms/" >&2
        exit 2
    fi
fi

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

if [ $status -eq 124 ]; then
    echo "FAILED: timed out after 300s" >&2
fi

exit $status
