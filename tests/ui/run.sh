#!/usr/bin/env bash
# Runs the widget suite. No board, no display server, no network.
#
#   ./run.sh                 everything
#   ./run.sh structure       one slot (any QTest filter works)
#   ./run.sh --gl    [slot]  only the OpenGL tier
#   ./run.sh --light [slot]  only the light-palette tier
#
# Naming a slot without a tier runs it in the offscreen tier only, so a slot
# that lives in one of the other two needs the flag -- otherwise it skips and
# reads as a pass. That is how the GL tier went unrun for the life of this
# suite: nothing in the dev shell provided xvfb-run, every GL row reported
# SKIP, and a skip is easy to read past.
#
# Plugin discovery is shared with the other suites -- see tests/qtenv.sh.

set -uo pipefail
cd "$(dirname "$0")"

. "$(dirname "$0")/../qtenv.sh"
qt_env_setup || exit 2

# The harness pins everything else itself, before the QApplication exists.
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

tier="all"
case "${1:-}" in
    --gl)    tier="gl";    shift ;;
    --light) tier="light"; shift ;;
esac

if [ ! -x ./tst_ui ]; then
    echo "run.sh: ./tst_ui is not built. qmake && make -j8" >&2
    exit 2
fi

# Stale mismatch files from a previous run must not survive into this one --
# update-baselines.sh promotes whatever is in actual/, and once swept a stale
# file turns a broken page into the baseline.
rm -rf actual

# A private network namespace when one can be had, unprivileged via a user
# namespace. Two reasons: UDP 65109 is then free, so the check that
# PageConnection does not bind it in its constructor actually runs instead of
# skipping past a VESC Tool the developer has open; and no device broadcast
# from the LAN can reach the page and change its widget tree mid-run.
#
# Loopback has to be brought up by hand inside the namespace, or the transport
# test cannot reach its own stub server.
NETNS=""
if unshare -rn true 2>/dev/null; then
    NETNS="yes"
fi

run_tests() {
    if [ -n "$NETNS" ]; then
        unshare -rn bash -c 'ip link set lo up 2>/dev/null; exec "$@"' _ "$@"
    else
        "$@"
    fi
}

status=0

# A hang must not read as a pass.
if [ "$tier" = "all" ]; then
    run_tests timeout 300 ./tst_ui "$@"
    status=$?
fi

# The light palette is a separate run: the colour tables for the two themes are
# independent lists in appstyle.cpp, and icon lookups resolve to a different
# directory, so a missing light-theme icon or colour is invisible to the dark
# run. Skipped when the caller asked for specific tests.
if [ $status -eq 0 ] && { [ "$tier" = "light" ] || { [ "$tier" = "all" ] && [ $# -eq 0 ]; }; }; then
    run_tests timeout 300 ./tst_ui --light "$@"
    status=$?
fi

# Six pages cannot be built without an OpenGL context: three host a
# QQuickWidget and three embed Vesc3DView. The offscreen platform reports no GL
# capability, so they run under a real X server with Mesa's software
# rasteriser. Skipped, not failed, when xvfb-run is unavailable -- the rest of
# the suite is still worth running.
#
# Started by hand rather than with xvfb-run, and on a display this script
# chooses, because both halves of the automatic path have bitten:
#
#   - xvfb-run -a picks a number and execs the command immediately, so under
#     load the client gets DISPLAY=:N before Xvfb has created the socket. Qt
#     then exits with "qt.qpa.xcb: could not connect to display :N" -- SIGABRT,
#     no summary line, and a stage reporting nothing but the word FAILED. Four
#     of eight runs in a container.
#   - Xvfb's own -displayfd is documented to write the number once the server
#     is ready, and that did not hold either: the number arrived and the
#     display was still unreachable.
#
# So: pick a display in a range nothing else uses, start Xvfb on exactly that
# one, and do not start the tests until a client has actually connected to it.
# A candidate that will not serve is skipped and the next is tried, which also
# self-heals against a socket left behind by a server that was killed.
#
# The range matters. Xvfb searching upward from :0 contends with whatever else
# is on the machine -- on a workstation with a desktop session it walks over
# :0 and :1 and reports "server already running" for both. A CI runner has
# none of that, but the tests should not behave differently depending on
# whether a desktop is present.
gl_display_free() {
    [ ! -e "/tmp/.X11-unix/X$1" ] && [ ! -e "/tmp/.X$1-lock" ]
}

gl_display_ready() {
    if command -v xdpyinfo >/dev/null 2>&1; then
        xdpyinfo -display ":$1" >/dev/null 2>&1
    else
        # The socket existing is a much weaker claim than a client having
        # connected -- it is true the instant Xvfb creates the file, which is
        # exactly the race this whole function exists to avoid. So warn, once:
        # without xdpyinfo the GL tier is back to being intermittent, and it
        # cost three container runs to work that out the first time. xdpyinfo
        # is in the dev shell; outside it, nix develop.
        if [ -z "${gl_probe_warned:-}" ]; then
            echo "  warning: xdpyinfo not found, so the X display cannot be" >&2
            echo "  probed and the GL tier may fail intermittently" >&2
            gl_probe_warned=1
        fi

        [ -S "/tmp/.X11-unix/X$1" ]
    fi
}

run_gl() {
    # The X unix socket lives here, and a container image need not have the
    # directory at all -- without it Xvfb starts and nothing can reach it.
    mkdir -p /tmp/.X11-unix 2>/dev/null || true

    local tmp
    tmp=$(mktemp -d)
    local rc=0
    local n
    local xvfb_pid=""
    local started=0

    for n in 71 72 73 74 75 76 77 78; do
        gl_display_free "$n" || continue

        Xvfb ":$n" -screen 0 1600x1200x24 >"$tmp/xvfb.out" 2>&1 &
        xvfb_pid=$!

        local i=0
        while [ $i -lt 100 ]; do
            gl_display_ready "$n" && { started=1; break; }
            kill -0 "$xvfb_pid" 2>/dev/null || break
            sleep 0.05
            i=$((i + 1))
        done

        [ "$started" -eq 1 ] && break

        kill "$xvfb_pid" 2>/dev/null
        wait "$xvfb_pid" 2>/dev/null
        xvfb_pid=""
    done

    if [ "$started" -ne 1 ]; then
        # An X server we could not start is a broken environment, not a broken
        # program, so this skips the way a missing Xvfb does rather than
        # reporting a test failure. Loud, with the server's own words, because
        # a silent skip is how this tier went unrun for the life of the suite.
        echo "  skipped: no usable X display, so the 6 GL pages were NOT tested" >&2
        sed 's/^/  xvfb: /' "$tmp/xvfb.out" >&2
        rm -rf "$tmp"
        return 0
    fi

    # QML2_IMPORT_PATH comes from qtenv.sh and has to survive into the child:
    # without it the QQuickWidget pages load no scene and the snapshots
    # silently record an empty view.
    DISPLAY=":$n" QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 \
        QT_PLUGIN_PATH="$QT_PLUGIN_PATH" \
        QML2_IMPORT_PATH="${QML2_IMPORT_PATH:-}" \
        timeout 300 ./tst_ui --gl "$@"
    rc=$?

    if [ $rc -ne 0 ]; then
        sed 's/^/  xvfb: /' "$tmp/xvfb.out" >&2
    fi

    kill "$xvfb_pid" 2>/dev/null
    wait "$xvfb_pid" 2>/dev/null
    rm -rf "$tmp"
    return $rc
}

if [ $status -eq 0 ] && { [ "$tier" = "gl" ] || { [ "$tier" = "all" ] && [ $# -eq 0 ]; }; }; then
    if command -v Xvfb >/dev/null 2>&1; then
        run_gl "$@"
        status=$?
    else
        # Not a failure, because the rest of the suite is still worth running
        # -- but loud, because this skipped silently for a long time. Xvfb is
        # in the dev shell now; outside it, nix develop.
        echo "  skipped: Xvfb not found, so the 6 GL pages were NOT tested" >&2
    fi
fi

if [ $status -eq 124 ]; then
    echo "FAILED: timed out after 300s" >&2
fi

exit $status
