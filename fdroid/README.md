# F-Droid

Nothing here has been submitted, and nothing here submits anything.

## What is in this directory

`io.github.siigna.escargot.yml` is a **draft** build recipe, kept as
documentation. It records what F-Droid would need in order to build this
fork, which is not the same as what `tests/android/build.sh` does.

## Why the recipe is not just our build script

`build.sh` fetches a prebuilt Qt with `aqtinstall`. F-Droid's inclusion
policy permits prebuilt binaries only from a short list of origins — Debian,
a few trusted Maven repositories, and specifically the Android, Flutter and
Hermes SDKs — and Qt is not among them.

The evidence for how to do it instead is empirical rather than written down:
the only currently live, actively updated Qt 5.15 application in the main
F-Droid repository builds Qt **from source** inside its own recipe, fetching
the official source tarball with a pinned `sha256sum -c -`. The one
application that downloaded prebuilt Qt binaries, with pinned digests, is
`disable:`d and archived. I did not read the merge request that disabled it,
so I cannot say that is why — but the from-source route is the one with a
working precedent.

## What is unverified in the draft

Written at the top of the recipe too, so it cannot be used without reading
them:

- the tarball digest is a placeholder
- the `-skip` list is untested against this application's modules, and
  skipping one it needs fails late
- whether a Qt build plus this application fits the default 7200 s timeout
- whether F-Droid's build server needs the `aapt2` override and the
  Gradle/AGP versions `build.sh` sets up, since the recipe calls
  `androiddeployqt` directly

## Still missing for a listing

- **Screenshots.** `fastlane/metadata/android/en-US/images/` has an icon and
  a feature graphic, both generated from `android/art/`, and no
  `phoneScreenshots/`. These have to come off a real device or an emulator;
  rendering the QML offscreen would produce something, but not something
  honest to put in a store listing.
- A decision on **who signs**: F-Droid's own key by default, or a
  reproducible build carrying ours. The latter needs a deterministic build
  this does not have — `VT_GIT_COMMIT` alone varies per commit.

## Anti-features to expect

Local-only GNSS logging should not attract `Tracking`, which the definition
scopes to reporting activity "to somewhere". The in-app update check would
have, and it is removed. Making the ride log opt-in — which it already is, it
starts only when the user enables it — is the hedge the documentation
suggests.
