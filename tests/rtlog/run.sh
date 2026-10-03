#!/usr/bin/env bash
# Runs the ride-logging suite. No controller, no display, no network beyond
# loopback.
#
#   ./run.sh
#   VT_VESCSIM=/path/to/vescsim ./run.sh
#
# Needs tests/vescsim from the firmware tree, which serves the VESC protocol
# over TCP using the firmware's own packet.c. Without it the suite skips, and
# says so: a silent skip is how the widget suite's OpenGL tier went unrun for
# its whole life.
#
# Plugin discovery is shared with the other suites -- see tests/qtenv.sh.

set -uo pipefail
cd "$(dirname "$0")"

. "$(dirname "$0")/../qtenv.sh"
qt_env_setup || exit 2

export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

if [ ! -x ./tst_rtlog ]; then
    echo "run.sh: ./tst_rtlog is not built. qmake && make -j8" >&2
    exit 2
fi

# Where a firmware checkout sits beside this one, unless told otherwise.
if [ -z "${VT_VESCSIM:-}" ]; then
    guess="$(cd ../.. && pwd)/../bldc/tests/vescsim/vescsim"
    if [ -x "$guess" ]; then
        export VT_VESCSIM="$guess"
    fi
fi

# Pinned before the binary starts, and the reason is specific:
# Utility::configPath registers $AppData/res_config.rcc over the compiled-in
# parameter XML, so a stale archive in a real home directory makes a working
# connection look like a protocol failure. That cost an hour once.
tmphome=$(mktemp -d) || exit 1
trap 'rm -rf "$tmphome"' EXIT

export XDG_CONFIG_HOME="$tmphome/config"
export XDG_DATA_HOME="$tmphome/data"
export XDG_CACHE_HOME="$tmphome/cache"
export HOME="$tmphome/home"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$HOME"

# A hang is a failure, not a pass: the suite waits on signals from a
# subprocess, and without this a dead simulator would sit until CI's own
# timeout with nothing useful to show.
timeout 180 ./tst_rtlog "$@"
st=$?

if [ "$st" -eq 124 ]; then
    echo "run.sh: timed out after 180s" >&2
fi

exit $st
