#!/usr/bin/env bash
# Creates the release signing key, outside the repository.
#
#   nix develop .#android --command tests/android/keystore.sh
#
# Then, to produce a signed APK:
#
#   export VT_ANDROID_KEYSTORE=~/.local/share/escargot/release.keystore
#   export VT_ANDROID_KEYSTORE_ALIAS=escargot
#   export VT_ANDROID_KEYSTORE_PASS=...          # or be prompted
#   nix develop .#android --command tests/android/build.sh mobile
#
# The key is deliberately NOT in the repository and not in the nix store.
# .gitignore covers *.keystore and *.jks as a second line of defence, but the
# real reason it lives under XDG_DATA_HOME is that a key in a build tree ends
# up in a tarball, a CI artifact or a nix store path sooner or later.
#
# What this key is and is not:
#
#   It signs APKs you build and install yourself. Android ties the signature
#   to the app id permanently, so a build signed with a different key cannot
#   update one signed with this key -- it has to be uninstalled first, losing
#   its data.
#
#   If F-Droid ever publishes this app, F-Droid signs it with ITS key by
#   default, which means an F-Droid install and a local install are mutually
#   exclusive in exactly that way. The alternative, where F-Droid reproduces
#   the build and ships your signature, needs a deterministic build this does
#   not have yet. That decision is deferred; see the plan.
#
#   Losing this key is survivable for a sideloaded app -- generate another and
#   reinstall. It is not survivable for a published one.

set -uo pipefail

dir="${VT_ANDROID_KEYSTORE_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/escargot}"
store="$dir/release.keystore"
alias_name="${VT_ANDROID_KEYSTORE_ALIAS:-escargot}"

if ! command -v keytool >/dev/null 2>&1; then
    echo "keystore.sh: no keytool on PATH. Run inside: nix develop .#android" >&2
    exit 2
fi

if [ -e "$store" ]; then
    echo "keystore.sh: $store already exists."
    echo "Refusing to overwrite it: a replaced key cannot sign an update to"
    echo "anything the old one signed. Delete it by hand if that is what you"
    echo "want."
    echo
    keytool -list -v -keystore "$store" 2>/dev/null \
        | sed -n '/Alias name/,/Valid from/p' | sed 's/^/  /'
    exit 1
fi

mkdir -p "$dir" || exit 1
chmod 700 "$dir"

echo "Creating $store"
echo "  alias:     $alias_name"
echo "  algorithm: RSA 4096, SHA256withRSA"
echo "  validity:  10000 days"
echo

# 10000 days because Google's own guidance is that a key should outlive the
# app; a certificate that expires strands every installed copy.
#
# -dname is given explicitly so the run is non-interactive and the fields are
# the same every time. It is a signing identity, not an assertion about a
# legal entity: nothing verifies it, and for a self-signed app nothing reads
# it except tooling.
keytool -genkeypair \
    -keystore "$store" \
    -storetype PKCS12 \
    -alias "$alias_name" \
    -keyalg RSA \
    -keysize 4096 \
    -sigalg SHA256withRSA \
    -validity 10000 \
    -dname "CN=ESCargot Tool, OU=ESCargot, O=ESCargot, C=US" || exit 1

chmod 600 "$store"

echo
echo "Done. To use it:"
echo
echo "  export VT_ANDROID_KEYSTORE=$store"
echo "  export VT_ANDROID_KEYSTORE_ALIAS=$alias_name"
echo "  export VT_ANDROID_KEYSTORE_PASS=<the password you just set>"
echo
echo "Back it up somewhere that is not this machine."
