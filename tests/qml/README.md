# QML tests

The mobile UI: 56 QML files, about 19,500 lines, which had no test of any
kind before this directory existed.

```
cd tests/qml && qmake && make -j8 && ./run.sh
```

## Why this is separate from `tests/ui`

The widget suite builds `QWidget` pages and walks their object trees. It
cannot reach any of this. `mobile/main.qml` is a different application entry
point -- `qmlui.cpp` loads it into a `QQmlApplicationEngine` instead of
constructing a `MainWindow` -- and the three desktop pages that do embed QML
are in the widget suite's GL tier only as a rendered snapshot.

So the mobile application was the largest untested surface in the fork, and
the one where the rebrand's own branding grep had to be a source-level grep
because no test could see the strings.

## What it checks

`tst_mobile_load.qml` -- every file in `mobile/qml.qrc` **compiles**. The file
list is read from the resource at run time, so a newly added QML file is
covered without this suite changing. QML is compiled when it is loaded, and
the mobile application loads `main.qml` and nothing else until the user
navigates, so a missing import or a typo in a property name is not a build
error and not a startup error -- it is a blank page three taps in.

`tst_mobile_components.qml` -- every file **instantiates**, and the engine
**warns about nothing**. This is the half that found defects. A binding loop
compiles and loads; what it costs is that Qt breaks the loop by refusing to
re-evaluate, so a size or position keeps whatever the aborted pass left
behind. Nothing crashes and nothing is logged where anyone looks.

It also pins two things narrower than a sweep: the firmware dialog's content
geometry, and `DoubleSpinBox`'s arithmetic -- Controls' `SpinBox` is integer
only, so every numeric motor parameter edited on a phone goes through that
component's scale factor of `10^decimals`.

## What it found

| file | defect |
|---|---|
| `SetupWizardFoc.qml` | `TabButton.width` read `tabBar.width`, closing a cycle through `TabBar.implicitWidth`; reported as two binding loops |
| `StartPage.qml` | `Dialog.implicitWidth` binding loop, from `FwUpdate.qml` carrying `anchors.fill: parent` on its own root while a `Dialog` sizes its `contentItem` itself |
| `SetupWizardIMU.qml` | `Connections` with an implicitly defined `onValuesImuReceived`, deprecated in Qt 5.15 and an error in Qt 6 |
| `DetectBldc.qml`, `DetectFocEncoder.qml`, `DetectFocHall.qml`, `DetectFocParam.qml` | the dialog's height read `column.height`, an id belonging to `ConfigPageMotor.qml`. It resolved only through the creation context, so the component worked from that one document and nowhere else |

All five are fixed. The last is the interesting one: four components depended
on an identifier from the document that happened to embed them, which no
compile step and no code review of the file itself can see.

## Two things that make a run lie

**The QML is a compiled-in resource.** `mobile/qml.qrc` is linked into the
test binary, so editing a `.qml` file changes nothing until `make` runs again.
`run.sh` refuses to run a `tst_qml` older than any file in `mobile/` rather
than report a pass for QML it never saw. This is the same trap
`tests/ui/update-baselines.sh` hit with the compiled-in parameter XML.

**The components are written against `main.qml`'s scope.** `MobileScope.qml`
supplies it -- an `ApplicationWindow` carrying `main.qml`'s own properties and
its `mainSwipeView` id -- and it is not a mock: without it, six components
report an undefined parent, an undefined `ApplicationWindow.overlay`, or an
unresolved `mainSwipeView`, none of which is a defect in the component. A
component that needs something missing from `MobileScope.qml` needs a thing
`main.qml` supplies, and it belongs there rather than in a test.

## `Vesc3DView.qml` does not load, deliberately

It imports `Qt3D.Core`, `Qt3D.Render`, `Qt3D.Input`, `Qt3D.Extras` and
`QtQuick.Scene3D`. `qt3d` is not among the Qt modules the package depends on
-- checked against the wrapper `nix build .#vesc-tool` produces, which carries
`qtquickcontrols`, `qtquickcontrols2`, `qtgraphicaleffects`, `qtpositioning`,
`qtgamepad`, `qtconnectivity`, `qtsvg` and `qtwayland`, and no `qt3d`.

So the file ships inside `mobile/qml.qrc` and can never load. Nothing
references it either: the only `Vesc3DView` the program uses is
`widgets/vesc3dview.cpp`, a `QOpenGLWidget`, which is a different class
entirely and is what the three desktop IMU pages embed. The QML one is dead.

It is left in place because it is upstream's file and this is a fork. The
state is recorded as a decision in `tst_mobile_load.qml`, which asserts it in
both directions -- if the file starts compiling, that test fails too, because
the reason written down would no longer be true.

## What this suite still does not cover

- **No Android build.** Nothing here compiles for Android or runs on a device
  or emulator; it exercises the same QML on the host. The Android
  configuration is checked separately and statically -- see
  `tests/android/README.md`, which also prices up what a real build job would
  cost -- but a break that only a compile for that target would show is
  invisible to both.
- **No rendered output.** `grabImage` works under the software backend, but
  there are no image baselines, so a component that loads cleanly and draws
  nothing passes.
- **No navigation.** Each component is created in isolation. The sequences a
  user actually walks -- the setup wizards in particular -- are not driven.
