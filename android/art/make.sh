#!/usr/bin/env bash
# Regenerates the Android launcher icons and splash images.
#
#   nix develop .#android --command android/art/make.sh
#
# The PNGs under android/res are committed, because the Android build has no
# SVG rasteriser and F-Droid will not run one. This script is what produced
# them, so they are reproducible rather than mystery binaries.
#
# Why they exist at all: the previous launcher icon was upstream's artwork.
# A published fork shipping upstream's logo is a trademark problem quite
# separate from the GPL, and it was also the last upstream asset left in the
# package. It was a single 192x192 PNG, byte-identical in the mdpi and
# xxhdpi buckets, with no adaptive icon.
#
# The mark is a logarithmic spiral, a snail shell for ESCargot, in this
# fork's own accent colour from appstyle.cpp. Deliberately placeholder
# quality: it is ours, it is legible at 48px, and it can be replaced without
# touching the application id.

set -uo pipefail
cd "$(dirname "$0")"

res="../res"

rsvg() {
    if command -v rsvg-convert >/dev/null 2>&1; then
        rsvg-convert "$@"
    else
        nix run nixpkgs#librsvg -- "$@"
    fi
}

python3 ./mark.py || exit 1

# Adaptive icon foreground: a 108dp canvas whose art stays inside the central
# 72dp, because the launcher masks and may animate anything outside it.
for d in "mdpi 108" "hdpi 162" "xhdpi 216" "xxhdpi 324" "xxxhdpi 432"; do
    set -- $d
    mkdir -p "$res/mipmap-$1" || exit 1
    rsvg -w "$2" -h "$2" icon_fg.svg -o "$res/mipmap-$1/icon_fg.png" || exit 1
done

# Legacy launcher icons, for API 25 and below, at 48dp.
for d in "mdpi 48" "hdpi 72" "xhdpi 96" "xxhdpi 144" "xxxhdpi 192"; do
    set -- $d
    mkdir -p "$res/mipmap-$1" || exit 1
    rsvg -w "$2" -h "$2" icon_legacy.svg -o "$res/mipmap-$1/icon.png" || exit 1
    rsvg -w "$2" -h "$2" icon_round.svg -o "$res/mipmap-$1/icon_round.png" || exit 1
done

# Splash images, replacing upstream's. Qt picks these by orientation; the
# names are the ones android/res/drawable/splashscreen*.xml already
# reference, so those files do not change.
rsvg -w 1500 -h 1000 splash_land.svg -o "$res/drawable/splash_land.png" || exit 1
rsvg -w 1000 -h 1500 splash_port.svg -o "$res/drawable/splash_port.png" || exit 1
cp "$res/drawable/splash_land.png" "$res/drawable/splash.png" || exit 1

# The xhdpi copies upstream shipped. Same images; Android picks whichever
# bucket it finds, and a missing one at a density it wants means no splash.
for f in splash splash_land splash_port; do
    cp "$res/drawable/$f.png" "$res/drawable-xhdpi/$f.png" || exit 1
done

printf 'regenerated:\n'
find "$res" -name 'icon*.png' -o -name 'splash*.png' | sort | sed 's/^/  /'
