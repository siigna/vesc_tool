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

## Where the ride log goes

Through the storage access framework, not a path. The user grants one
directory once, Android remembers the grant, and no storage permission is
involved at all.

| | |
|---|---|
| picker | `Utility::pickLogDirectory` launches `ACTION_OPEN_DOCUMENT_TREE` via `QtAndroid::startActivity`, which already carries a result callback |
| grant | `Utils.takeTreePermission` makes it survive a reboot; if that fails **nothing is stored**, because a grant that works once and then dies mid-ride is worse than being unset |
| liveness | `Utils.hasTreePermission` — a user can revoke it in settings, and the volume can be unmounted |
| the file | `Utils.createLogFile` creates a `text/csv` document and returns a detached fd |
| the write | `VescInterface::openRtLogFileSaf` opens that fd with `AutoCloseHandle`, so `closeRtLogFile` is unchanged and the fd cannot leak on an error path |

The CSV header and the position-source startup are in one `finishRtLogOpen`
shared by both open paths. A header written in two places drifts, and that
format is what the desktop log analysis page parses.

`mRtLogName` exists because an fd-backed `QFile` has no file name, so
`QFileInfo::canonicalFilePath` returns nothing and `rtLogFilePath` had nothing
to show the user.

### What this did not fix

`Utility::requestFilePermission` still returns `true` and requests nothing.
The ride log no longer goes through it, but ten other callers do — firmware
files, configuration backups, Lisp sources — and they still address storage by
path. Those work where the path is app-private, which is where `FilePicker`
starts, and do not where it is not. Its comment now says that rather than
"Not working since android 13" sitting above `return true`.

Moving them onto the same framework is separate work and is not done.

## Running it: tests/android/emulator.sh

```
nix develop .#emulator --command tests/android/emulator.sh
nix develop .#emulator --command tests/android/emulator.sh --keep
```

The first thing here that runs the application rather than building it. API 31
x86_64, which matches `VT_ANDROID_TARGET_SDK`, under KVM. The Qt kit already
carries `x86_64` -- the 5.15 Android download is multi-ABI -- so this costs no
second Qt download.

It builds, signs with a **throwaway** key generated for the run and discarded
with it, boots a fresh AVD, installs, launches, and judges the result by
logcat. The release key from `keystore.sh` is never involved: a test harness
should not need, touch, or be able to use the key that signs what goes on a
device.

### What it established, that nothing else could

- The application installs, launches and keeps running on Android 12, with
  no crash markers.
- **Zero QML errors from `qrc:/mobile` or `qrc:/res`.** The 906 that do appear
  are all `qrc:/android_rcc_bundle` -- Qt's own Quick Controls 1 Android
  style, pulled in because `FilePicker.qml` and `DirectoryPicker.qml` import
  `QtQuick.Controls 1.4`.
- **The repackaged JNI resolves at runtime.** `Utils.checkLocationEnabled`
  returned false and the app surfaced "BLE scan does not seem to be
  possible", which means `io/github/siigna/escargot/Utils` was found. Those
  are string literals the compiler cannot check, and renaming the package was
  the highest-risk change in this work.
- The BLE permission path fires: the system nearby-devices dialog appears,
  naming the application correctly.
- `maxSdkVersion="30"` on `BLUETOOTH` behaves -- it is absent on API 31.
- The rebrand renders everywhere it is visible at runtime: dialogs, the system
  permission prompt, the title.
- The background-location disclosure is shown **before** the runtime prompt,
  which is what it has to be.

### What it cannot reach, and why

**`POST_NOTIFICATIONS` does not exist on API 31.** It is API 33 and later, so
`pm grant` rejects it as an unknown permission here. Testing the notification
path needs an API 33 or 34 image. Note also that an application targeting
below 33 is not prompted for it at all.

**The storage access framework picker and background logging are behind a
working controller connection.** `LogBox` lives in `StartPage.qml`, and the
connect screen is the only page until a connection exists -- verified by
walking the mandatory intro wizard and then finding that nothing swipes. So
`pickLogDirectory` and `createLogFile` have still never executed.

Two ways to change that, neither done:

1. Point the emulator at a real controller over TCP. Exercises the real
   protocol and real data, and needs the bench rig powered.
2. A VESC protocol stub that answers enough of `COMM_FW_VERSION` for
   `VescInterface` to consider itself connected. More work, but it would make
   the whole logging path testable headlessly, including in CI.

**UI automation is limited.** Qt does bridge accessibility -- `uiautomator`
sees `BLE scan` and dialog titles as `content-desc` -- but most controls set
no `Accessible.name`, so they are invisible to it and the dialog buttons are
not addressable by text. Driving the app past the wizard needed pixel taps
read off screenshots, which is fine for an investigation and not a basis for
a durable test. Adding `Accessible.name` to the handful of controls a test
would touch is the honest fix, and has its own benefit for screen readers.

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
