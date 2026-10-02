#!/usr/bin/env bash
# The command-line surface, as a process rather than as widget code.
#
#   ./run.sh
#
# These flags live in main.cpp and none of them is reachable from the widget
# tests: the screenshot path, the offline payload dump, the argument checking,
# and the failure a closed port produces. No board and no display are needed --
# the screenshot path runs offscreen, which is the whole point of it.

set -uo pipefail
cd "$(dirname "$0")"

. ../qtenv.sh
qt_env_setup || exit 2

APP=$(ls -t ../../build/lin/vesc_tool_* 2>/dev/null | head -1)

if [ -z "$APP" ] || [ ! -x "$APP" ]; then
    echo "run.sh: no built application under build/lin." >&2
    echo "  qmake -config release \"CONFIG += release_lin build_original exclude_fw\" && make -j8" >&2
    exit 2
fi

# Hermetic: nothing here may read or write the developer's settings. intro_done
# keeps MainWindow's startup checks from opening the introduction wizard.
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/config/VESC" "$T/data"
printf '[General]\ndarkMode=true\nintro_done=true\nintroVersion=1\n' \
    > "$T/config/VESC/VESC Tool.conf"

export XDG_CONFIG_HOME="$T/config"
export XDG_DATA_HOME="$T/data"
export LC_ALL=C

pass=0
fail=0

ok()   { printf '  ok      %s\n' "$1"; pass=$((pass + 1)); }
bad()  { printf '  FAILED  %s\n' "$1"; [ -n "${2:-}" ] && printf '            %s\n' "$2"; fail=$((fail + 1)); }

# Both streams to files, always. Mixing "capture stdout" with "grep stderr"
# and bare $? is how the first version of this reported a failure for a case
# that was in fact behaving correctly.
run() {
    timeout 120 "$APP" "$@" >"$T/out" 2>"$T/err"
    status=$?
    return $status
}

# Did the program say this, on either stream?
saw() { grep -q -- "$1" "$T/out" "$T/err" 2>/dev/null; }

# What it said, for a failure message.
said() { grep -hvE 'propagateSizeHints|^DEBUG' "$T/err" "$T/out" 2>/dev/null | tail -2 | tr '\n' ' '; }

# A log in the package logger's shape, with location columns present on
# purpose: the dump has to drop them.
LOG="$T/sample.csv"
{
    printf 'Input Voltage:Input Voltage:V:2:0:0;RPM:RPM::2:0:0;'
    printf 'kmh_vesc:Speed ESC:km/h:2:0:0;'
    printf 'gnss_lat:gnss_lat::2:0:0;gnss_lon:gnss_lon::2:0:0\n'
    for i in $(seq 1 20); do
        printf '80.0;%d;%d;57.70887;11.97456\n' "$((i * 100))" "$i"
    done
} > "$LOG"

# --- the prompt is printable, so it can be edited and handed back ----------
run --insightsPrintPrompt
if [ $status -eq 0 ] && [ "$(wc -c <"$T/out")" -gt 200 ] && saw "tuning observations"; then
    ok "--insightsPrintPrompt writes the instructions"
else
    bad "--insightsPrintPrompt" "exit $status, $(wc -c <"$T/out") bytes"
fi

# --- the offline dump, which is how anyone checks the redaction themselves --
run --tuningInsights --dryRun --insightsOffline --insightsLog "$LOG" --insightsMaxRows 5

if [ $status -ne 0 ]; then
    bad "--insightsOffline --dryRun" "exit $status: $(said)"
elif ! python3 -c 'import json,sys; json.load(open(sys.argv[1]))' "$T/out" 2>/dev/null; then
    bad "--insightsOffline --dryRun" "stdout is not valid JSON"
elif grep -qi gnss "$T/out"; then
    bad "--insightsOffline --dryRun" "payload contains gnss"
elif grep -q '57\.70887' "$T/out"; then
    bad "--insightsOffline --dryRun" "payload contains coordinates"
elif ! grep -q 'kmh_vesc' "$T/out"; then
    bad "--insightsOffline --dryRun" "payload has no log columns at all"
else
    ok "--insightsOffline --dryRun emits a payload with no location"
fi

# --- the refusals, each of which must say why and not exit 0 ---------------
run --tuningInsights --insightsOffline --insightsLog "$LOG"
if [ $status -ne 0 ] && saw "only makes sense with --dryRun"; then
    ok "--insightsOffline without --dryRun is refused"
else
    bad "--insightsOffline without --dryRun is refused" "exit $status: $(said)"
fi

run --tuningInsights --dryRun --insightsOffline
if [ $status -ne 0 ] && saw "needs --insightsLog"; then
    ok "--insightsOffline without a log is refused"
else
    bad "--insightsOffline without a log is refused" "exit $status: $(said)"
fi

run --tuningInsights --dryRun --insightsOffline --insightsLog "$LOG" --insightsMaxTokens 0
if [ $status -ne 0 ] && saw "positive number"; then
    ok "--insightsMaxTokens 0 is refused"
else
    bad "--insightsMaxTokens 0 is refused" "exit $status: $(said)"
fi

run --tuningInsights --insightsProvider definitely-not-a-provider --insightsOffline --dryRun --insightsLog "$LOG"
if [ $status -ne 0 ] && saw "Unknown provider"; then
    ok "an unknown provider is named as such"
else
    bad "unknown provider" "exit $status: $(said)"
fi

# --- the screenshot, which is the reason --offscreen had to start working ---
shot="$T/shot.png"
run --showPage "Tuning Insights" --screenshot "$shot" --screenshotSize 1400x900

if [ $status -ne 0 ]; then
    bad "--screenshot" "exit $status: $(said)"
elif [ ! -s "$shot" ]; then
    bad "--screenshot" "wrote no file"
else
    # Dimensions and a sane size: a 900-wide window that renders nothing would
    # still produce a file, so check the width and that it is not a blank sliver.
    read -r w h < <(python3 - "$shot" <<'PY'
import struct, sys
with open(sys.argv[1], 'rb') as f:
    head = f.read(26)
w, h = struct.unpack('>II', head[16:24])
print(w, h)
PY
)
    bytes=$(stat -c%s "$shot")
    if [ "$w" != "1400" ]; then
        bad "--screenshotSize" "asked for 1400 wide, got ${w}x${h}"
    elif [ "$bytes" -lt 10000 ]; then
        bad "--screenshot" "only $bytes bytes; the window probably rendered nothing"
    else
        ok "--screenshot renders ${w}x${h} offscreen ($((bytes / 1024)) kB)"
    fi
fi

# --- a size the window cannot honour must be reported, not silently changed --
run --showPage "Tuning Insights" --screenshot "$T/small.png" --screenshotSize 400x300
if [ $status -eq 0 ] && saw "will not go smaller"; then
    ok "a size below the window minimum is reported"
else
    bad "--screenshotSize below the minimum" "exit $status: $(said)"
fi

# --- a control that does not exist must fail, not capture the wrong thing --
run --showPage "Tuning Insights" --screenshotClick noSuchButton --screenshot "$T/x.png"
if [ $status -ne 0 ] && saw "No control named" && [ ! -e "$T/x.png" ]; then
    ok "--screenshotClick names a control it cannot find, and writes nothing"
else
    bad "--screenshotClick with a bad name" "exit $status: $(said)"
fi

# --- a closed port must fail promptly and say so --------------------------
start=$(date +%s)
run --vescTcp 127.0.0.1:1 --tuningInsights --dryRun
elapsed=$(( $(date +%s) - start ))

if [ $status -eq 0 ]; then
    bad "--vescTcp to a closed port" "exited 0"
elif [ $elapsed -gt 30 ]; then
    bad "--vescTcp to a closed port" "took ${elapsed}s to give up"
elif ! saw "Could not connect" && ! saw "Could not read firmware"; then
    bad "--vescTcp to a closed port" "$(said)"
else
    ok "--vescTcp to a closed port fails in ${elapsed}s and says why"
fi

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ $fail -eq 0 ]
