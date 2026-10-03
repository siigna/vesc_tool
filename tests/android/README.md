# Android

## What runs here

`run.sh` checks the Android build configuration with no SDK, no NDK and no
device:

- every `android/` path in `DISTFILES` exists. qmake treats `DISTFILES` as
  informational, so a stale entry is never a build error — which is how
  `android/gradlew.bat` stayed in that list, absent from this tree and from
  upstream's.
- `ANDROID_PACKAGE_SOURCE_DIR` exists.
- every qmake variable in `AndroidManifest.xml.in` is defined.
- the template **substitutes into well-formed XML**, and that result has the
  right namespace, an integer `versionCode`, this fork's own package id, a
  target SDK inside Qt 5.15's range, exactly the expected permission set, and
  a non-exported location service.
- every component the manifest declares from this project's own Java packages
  has a `.java` file behind it.

## The template is not XML, and that is correct

Every value in `AndroidManifest.xml.in` is wrapped in `'"..."'`, the XML
declaration included:

```xml
<?xml version='"1.0"'?>
<manifest package='"$${VT_ANDROID_PACKAGE}"' ...>
```

`QMAKE_SUBSTITUTES` strips the single quotes and leaves the double ones, so
qmake writes `version="1.0"` and `package="io.github.siigna.escargot"`. Write
the values with plain double quotes and the output is `version=1.0`, which is
not XML at all.

**An earlier version of this check got this backwards.** It parsed the
template as XML, found the declaration ill-formed and the `android:`
attributes outside the namespace, concluded the manifest was unusable, and
asserted that it stay that way — reading the input of a substitution as
though it were the output. The fix was verified the only way it could be: run
qmake over the template and parse what it writes.

`manifest.py` now applies the same two transformations qmake applies and
asserts on the result. That is an emulation, and two limits come with it:
variables are not resolved recursively (`VT_ANDROID_VERSION` is a
`$$replace()` on `VT_VERSION`, which only qmake can evaluate), and where a
variable has two assignments it takes the first — so it tests the
`build_mobile` variant's package id, not the full one. The real generated
manifest is asserted against a built APK with `aapt2 dump badging`.

## Identity

| | |
|---|---|
| package | `io.github.siigna.escargot`, and `.full` for the widget-UI variant |
| Java | `io.github.siigna.escargot` for both, shared |
| versionCode | derived: `VT_VERSION` 7.02 → 702 |
| minSdk / targetSdk | 23 / 31 |

The Java package stays the same across both variants while the application id
differs, so `VForegroundService` cannot `import <pkg>.R` — the generated `R`
follows the manifest package. It resolves its icon with
`getResources().getIdentifier(...)` instead, which makes the source
independent of which id is running.

The JNI class strings in `utility.cpp` (`io/github/siigna/escargot/Utils`,
seven of them) are string literals the compiler cannot check. `manifest.py`
catches a renamed *component*; a renamed `Utils` would only show up at
runtime. Grep for the old spelling when moving Java.

targetSdk is **31**, not the 35 the template used to claim. Qt 5.15 supports
API 21 to 31; above that nothing has validated Qt's Java bindings or the
Gradle it ships. F-Droid imposes no floor of its own, and the only floor that
exists is Android refusing to install below API 23.

## Permissions

Asserted as an exact set, so one cannot be added without a reason or lost by
accident. Each has a call site:

| permission | where |
|---|---|
| `BLUETOOTH` (maxSdk 30), `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` | `bleuart.cpp:372-396` requests the latter two before constructing the discovery agent |
| the three location permissions | `vescinterface.cpp:1858-1879` starts a `QGeoPositionInfoSource` when RT logging opens |
| `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_LOCATION` | `VForegroundService`, started from `mobile/LogBox.qml:161` |
| `POST_NOTIFICATIONS` | that service's notification, which is the only way to see or stop logging |
| `WAKE_LOCK` | `vescinterface.cpp:115-134` |

Dropped, with reasons: `READ_`/`WRITE_EXTERNAL_STORAGE` and
`requestLegacyExternalStorage` (legacy storage is ignored from API 30, so the
combination worked on no phone this would be installed on; logs move to a
user-picked folder instead), `VIBRATE` (nothing in the tree vibrates) and
`SET_ORIENTATION` (signature-level, never granted to an app).

## Building

```
nix develop .#android --command tests/android/build.sh mobile
nix develop .#android --command tests/android/build.sh full
nix develop .#android --command tests/android/build.sh --reuse mobile
```

Two applications from one tree, installable side by side:

| variant | package | label | UI |
|---|---|---|---|
| `mobile` | `io.github.siigna.escargot` | ESCargot Tool | the QML phone UI |
| `full` | `io.github.siigna.escargot.full` | ESCargot Tool Desktop | the widget UI |

Each is a single universal APK carrying `arm64-v8a` and `armeabi-v7a`, about
61 MB, **unsigned** unless `VT_ANDROID_KEYSTORE` and
`VT_ANDROID_KEYSTORE_ALIAS` are set — see `keystore.sh`. Upstream's
`build_android` harvested the debug-signed APK out of `outputs/apk/debug/`,
which is how a debug key ends up shipped; this produces a release APK and
signs it only when asked.

`--reuse` keeps the compiled objects, because compiling two ABIs takes about
fifteen minutes. It still re-runs qmake, which is not optional: qmake writes
the deployment settings that carry the SDK and build-tools versions, so
skipping it makes a toolchain change silently ineffective.

### Where the pieces come from

nix provides the SDK (platform 31, build-tools **30.0.3**), NDK
`21.4.7075529` (r21e) and JDK 11, with the unfree licence scoped to a
separate nixpkgs instance. Qt 5.15.2 is fetched by `aqtinstall` into
`$XDG_CACHE_HOME/escargot/qt-android` — nixpkgs has no Qt for Android — and
that download is the one non-reproducible input.

Three things about that environment are not obvious:

- **The Qt host tools cannot run as downloaded.** They are generic-linux
  x86-64 ELF wanting `/lib64/ld-linux-x86-64.so.2`. `build.sh` rewrites the
  interpreter and rpath of the 27 executables in `bin/` and nothing else:
  `lib/` is ARM, and is the build output rather than a tool.
- **AGP downloads its own `aapt2`**, equally unrunnable, and reports it as
  `Daemon startup failed ... This should not happen under normal
  circumstances`. It does, here. `android.aapt2FromMavenOverride` points AGP
  at the SDK's own `aapt2`, which nixpkgs has patched. That setting lives in
  a gradle home under the cache, because androiddeployqt regenerates the
  project `gradle.properties` on every run.
- **build-tools is 30.0.3, not 31.0.0.** Build-tools 31 removed `dx` in
  favour of `d8`, and AGP 4.2.2 validates an install by looking for `dx`, so
  31.0.0 fails with `Installed Build Tools revision 31.0.0 is corrupted` —
  which is untrue. `compileSdkVersion` comes from the platform, so this costs
  nothing.

Gradle is 6.7.1 with AGP 4.2.2: the oldest pair that accepts JDK 11, which
Qt 5.15.8+ requires. Upstream had AGP 3.2.0 on Gradle 4.6 against `jcenter()`,
and Gradle 4.6 cannot parse a JDK 11 version string at all.

`lintVitalRelease` is disabled. AGP 4.2's lint crashes inside
`LintCliClient.createLintRequest` and reports `Failed to parse XML` about a
manifest that is clean ASCII, balanced, duplicate-free and parsed
successfully by AGP's own manifest merger earlier in the same build. Note
that `abortOnError false` does not cover it; `checkReleaseBuilds false` does.

### Editing the manifest template

Beyond the quoting above, two rules, both found by breaking them:

- **No apostrophes, anywhere, prose included.** qmake reads one as an opening
  quote and swallows the rest of the line. A comment containing
  `the service` + `'s own notification` lost its whole comment-opening line,
  leaving an orphan close that Python parsed happily and Android lint did
  not. This is why upstream's prose reads oddly.
- **No double hyphen inside a comment.** A parse error in XML.

`manifest.py` checks both, and caught three attempts to write these rules
*into* the template.

## The permission set is asserted twice, and only one of them is the artefact

`manifest.py` checks the template and what qmake writes. `apk.py` checks what
`aapt2 dump badging` says about the built package, and imports the expected
permission set from `manifest.py` so the two cannot drift.

Both are needed, and the reason is concrete: the first APK that built
declared `minSdkVersion 1` while every manifest check correctly reported 23.
AGP replaces the manifest `uses-sdk` with its `defaultConfig`, and an unset
`defaultConfig.minSdkVersion` defaults to 1. The manifest was right and the
package was wrong, and only the package could say so.

The SDK levels are now plumbed rather than assumed: `app.pri` sets
`ANDROID_MIN_SDK_VERSION` and `ANDROID_TARGET_SDK_VERSION` from the same
`VT_ANDROID_*` variables the template uses, qmake puts them in the deployment
settings, androiddeployqt turns them into `qtMinSdkVersion` and
`qtTargetSdkVersion`, and `defaultConfig` reads those.

`apk.py` is mutation-tested against real badging output: `minSdk 1`,
`WRITE_EXTERNAL_STORAGE` injected back, an upstream label, a single ABI, and
an upstream package id are all caught, each with a message naming the cause.

## What a build still does not prove

No emulator and no device, so nothing here shows that it runs. The things
only a phone can answer: whether BLE finds a controller, whether the RT data
screen draws, whether a ride log survives the app being backgrounded with the
screen off, and whether the foreground-service notification appears and stops
the log.

## The clock on Qt 5.15

Worth stating because it is not a store policy and will not go away: Google
requires 16 KB memory page support for apps targeting API 35+, and from
Android 17 a 4 KB-aligned binary **aborts** rather than warning.

Qt 5.15 has no 16 KB support from any vendor. `app.pri:70-72` already passes
`-Wl,-z,max-page-size=16384`, but that aligns only our own objects — every
bundled `libQt5*.so` would need the same, and NDK r21's `libc++_shared.so` is
not aligned and has no aligned canary build (those exist for r23 and r25).

So this is a Qt 6 port eventually, where 16 KB works out of the box from
6.10. The cost is known and large: `QtQuick.Extras` has no Qt 6 successor and
both gauge components use it, `QtQuick.Controls 1`, `QtQuick.Controls.Styles`,
`QtGraphicalEffects`, `Qt.labs.folderlistmodel` and `QtQuick.Dialogs 1.x`
appear across 17 more QML files, and roughly 30 `QtAndroidExtras` call sites
become `QJniObject`. Not now, but not never.
