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

## Still missing: a build

Nothing here compiles for Android. A build job would catch what these checks
cannot — a `.pro` that no longer configures, a Qt module unavailable for the
Android kit, Java that does not compile, a manifest the packaging step
rejects.

- **nixpkgs has no Qt for Android**, only host Qt 5.15.19. `androidenv` does
  provide the SDK, NDK and build-tools, so only Qt itself has to come from
  elsewhere.
- Qt 5.15 wants **NDK r20b or r21** and JDK 11, per
  `doc.qt.io/qt-5/android-getting-started.html`.
- **Qt Serial Port is not supported on Android in Qt 5.15**, which is why
  `app.pri:113` excludes `HAS_SERIALPORT` there. No USB serial on a phone.

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
