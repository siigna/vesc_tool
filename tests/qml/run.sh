#!/usr/bin/env bash
# Runs the QML suite against the mobile UI. No board, no display server, no
# network.
#
#   ./run.sh                          everything
#   ./run.sh tst_mobile_load.qml      one file
#   ./run.sh MobileComponents         one TestCase (any QTest filter works)
#
# Plugin and QML import discovery is shared with the other suites -- see
# tests/qtenv.sh. The QML import path is the part the widget suite does not
# need: every mobile file imports QtQuick.Controls, and the gauges also want
# QtQuick.Extras from Qt Quick Controls 1.
#
# THE QML IS A COMPILED-IN RESOURCE. mobile/qml.qrc is linked into the test
# binary, so editing a .qml file changes nothing until the binary is rebuilt.
# This is the same trap tests/ui/update-baselines.sh hit with the parameter
# XML, so run.sh refuses to run a binary older than the QML it is meant to be
# testing rather than reporting a stale pass.

set -uo pipefail
cd "$(dirname "$0")"

. "$(dirname "$0")/../qtenv.sh"
qt_env_setup || exit 2

export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

# Qt Quick needs a scene graph backend, and the offscreen platform reports no
# OpenGL capability at all -- without this the binary aborts before the first
# test. LIBGL_ALWAYS_SOFTWARE does not help, because the problem is the
# platform plugin, not the driver.
export QT_QUICK_BACKEND=${QT_QUICK_BACKEND:-software}

if [ ! -x ./tst_qml ]; then
    echo "run.sh: ./tst_qml is not built. qmake && make -j8" >&2
    exit 2
fi

# Refuses a stale binary rather than reporting a pass for QML it never saw.
newer=$(find ../../mobile -name '*.qml' -newer ./tst_qml -print -quit 2>/dev/null)
if [ -n "$newer" ]; then
    echo "run.sh: $newer is newer than ./tst_qml." >&2
    echo "        The QML is linked in through mobile/qml.qrc; run make." >&2
    exit 2
fi

# A hang must be a failure, not a pass: a QML test that waits on a signal that
# never arrives otherwise sits there until CI's own job timeout, which reports
# nothing useful.
timeout 300 ./tst_qml "$@"
st=$?

if [ "$st" -eq 124 ]; then
    echo "run.sh: timed out after 300s" >&2
fi

exit $st
