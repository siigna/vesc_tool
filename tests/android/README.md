# Android

Two different things are missing, and this directory is only the cheap one.

## What runs here

`run.sh` checks the Android build configuration with no SDK, no NDK and no
device, because none of it was checked at all before:

- every `android/` path in `DISTFILES` exists. qmake treats `DISTFILES` as
  informational, so a stale entry is never a build error -- which is how
  `android/gradlew.bat` stayed in that list, absent from this tree and from
  upstream's, for as long as the list has existed.
- `ANDROID_PACKAGE_SOURCE_DIR` exists. It is what `androiddeployqt` copies
  wholesale.
- every qmake variable in `AndroidManifest.xml.in` is defined. Substitution
  turns an undefined one into an empty attribute rather than an error, and an
  empty `android:versionCode` is a manifest the Play Store rejects.
- the template parses as XML, and its quoting is uniform (see below).
- every component the manifest declares from this project's own Java packages
  has a `.java` file behind it. A declared class with no source is a crash the
  moment Android tries to start it.

## The manifest is not a usable manifest

Every value in `AndroidManifest.xml.in` is wrapped in a second pair of quotes,
which is upstream's spelling and unchanged here:

```xml
<?xml version='"1.0"'?>
<manifest xmlns:android='"http://schemas.android.com/apk/res/android"'
          android:versionCode='"$${VT_ANDROID_VERSION}"' ...>
```

This is not cosmetic. The XML declaration is ill-formed, because a
`VersionNum` cannot contain a quote. Every attribute value carries the quotes
into its *value*, so `android:versionCode` is the string `"1"` and not the
integer `1`. And the `xmlns:android` URI is itself quoted, so none of the
`android:` attributes are in the Android namespace -- ask the parsed tree for
`android:versionCode` and it returns nothing.

So whatever produces upstream's Android releases is not this file as it
stands. The history around it says the same: *"Another attempt at checking in
the correct file..."*.

It has not been rewritten. Seventy-nine attributes cannot be re-quoted and
called correct without an Android build to try it against. What `manifest.py`
asserts instead is that the file stays **uniform** -- all values quoted, which
is today's state, or none, which is the fixed state. A mixed file is a hand
edit that did not decide which, and that is the failure worth catching.

## What is still missing: an actual build

Nothing here compiles for Android. A build job would catch what these checks
cannot -- a `.pro` that no longer configures, a Qt module unavailable for the
Android kit, a Java source that does not compile, a manifest the packaging
step rejects -- and it is the only thing that would settle the question above.

The cost, so the decision is on the table rather than implied:

- **It cannot use this flake.** nixpkgs has no Qt 5 for Android, so the job
  would provision Qt from the official installer (`install-qt-action` with an
  `android` target), which is Qt 5.15.2 rather than the 5.15.19 nixpkgs
  carries here. That is a second, divergent Qt in CI.
- **SDK and NDK.** Several gigabytes, and a licence acceptance step.
- **Modules.** The application wants `androidextras`, `bluetooth`, `gamepad`,
  `positioning` and `serialport` for the Android kit, not just the desktop
  one.
- **It proves a compile, not a run.** No emulator, so none of the QML this
  fork now tests on the host would be exercised on a device. The QML suite in
  `tests/qml` is the part that tests behaviour; an Android job would be a
  build check.

`vesc_tool.pro` also still carries upstream's Android application id,
`vedder.vesctool`, and the Java package `com.vedder.vesc`. Both are left
alone deliberately: they are identity rather than display strings, and
changing either makes the build a different app that cannot upgrade an
installed one.
