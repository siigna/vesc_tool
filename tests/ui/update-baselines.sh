#!/usr/bin/env bash
# Accepts the current widget structure as the new baseline.
#
# Run this deliberately, after a UI change you meant to make, and commit the
# resulting diff alongside that change -- the diff is the record of what the
# change did to the interface. Never run it to make a red suite go green
# without reading what moved.

set -uo pipefail
cd "$(dirname "$0")"

mkdir -p baseline

# Clear actual/ first. Without this the script promotes leftovers from earlier
# runs: a mutation test left an actual/PageAppPas.json with zero parameter rows
# behind, and a later update swept it into the baseline, so the suite went
# green against a snapshot of a deliberately broken page.
rm -rf actual

# The suite writes actual/<page>.json for every mismatch, so one run produces
# everything that needs updating.
./run.sh >/dev/null 2>&1

if [ ! -d actual ] || [ -z "$(ls -A actual 2>/dev/null)" ]; then
    echo "Nothing to update: every snapshot already matches its baseline."
    exit 0
fi

for f in actual/*.json; do
    name=$(basename "$f")
    if [ -f "baseline/$name" ]; then
        echo "updated  $name"
    else
        echo "new      $name"
    fi
    mv "$f" "baseline/$name"
done

rmdir actual 2>/dev/null || true
echo
echo "Re-run ./run.sh to confirm, then commit baseline/ with the change."
