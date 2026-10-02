# Widget tests

```sh
cd tests/ui && qmake && make -j8 && ./run.sh      # or: ./tests/check.sh
```

No board, no display server, no network. 25 pages and 7 behaviour checks run in
well under a second, because nothing here needs a window manager or a synthetic
mouse: a page is a `QWidget` with a one-argument constructor, and the parameter
XML is compiled into the binary.

That is the point of this directory. Capturing one screenshot of a page by
driving the real application under Xvfb with `xdotool` took over six minutes
and was wrong three times, because the navigation list does not always scroll
to the same row.

## What is checked

**`structureMatchesBaseline`** — each page's widget tree against a committed
JSON baseline in `baseline/`. Records object name, class, enabled state, the
text that identifies a control, combo items, tab labels, and for each
`ParamTable` the row count *and the editor class per row*. The last of those is
what notices a parameter being renamed out of existence: `addParamRow` returns
false and the row simply does not appear, with nothing reported anywhere.

Editors and their internals are skipped. `ConfigParams::getEditor` creates them
without an object name, so Qt names each after its class, and listing them gave
a dozen indistinguishable `ParamEditDouble` entries per page that buried
everything else. Their identity lives in the table's `editors` list instead.

A **missing** baseline is a failure, not a pass — otherwise a renamed page would
quietly stop being checked.

**`paramPagesAreNotEmpty`** — the parameter XML actually loaded and the pages'
subgroup names still match it.

**`paramEditorKeepsItsValue`** — a value driven into an editor survives the trip
out to `ConfigParams` and back. This is the guarantee that a redesign has not
changed what gets written to a controller.

**`editorWritesThroughToConfig` / `configChangeReachesEditor`** — driving an
editor changes the backing `ConfigParams`, and changing `ConfigParams` moves the
editor. Both ask the editor for the parameter it is bound to via
`ParamEditDouble::name()`; an earlier version matched parameter to editor by
equal value and picked `app_ppm_conf.ramp_time_neg`, which is not on the page
under test at all.

**`snapshotsAreStable`** — the same page described twice, with a real wait in
between, must come out the same. Only the eight pages that start a timer in
their constructor; nothing else can drift, and waiting on all thirty-two cost
twenty-two seconds for nothing. This separates "the page changed" from "the
page changes on its own", which otherwise shows up as an unreproducible
failure against the baseline.

It found a real one immediately: `PageEspProg` ships `eraseLispButton` and
others *enabled* in its `.ui`, and its 50 ms timer disables them because no ESP
is attached. The committed baseline had recorded a state that exists for 50 ms
and that nobody ever sees. The structure snapshot now waits before describing,
so the baseline is the settled page.

**`noMissingIconsOrColours`** — constructs every page with the Qt message
handler captured and fails on `icon not found` or `not found in standard
colors`. Both failures are otherwise invisible: a missing icon draws nothing
(so a light-theme variant nobody added looks fine in dark mode) and an unknown
colour name comes back red. `Utility::getIcon` was made to warn for this;
`getAppQColor` already did, at debug level.

**`brandingIsOurs`** — the logo and icon resources load in both themes, the
about box names this fork and carries the placeholder logo's CC BY-SA credit,
and no page label calls this program by the upstream name.

**The `insightsSaves*` checks** — the saves, driven through
`saveSentLogTo`/`saveBundleTo` rather than the buttons. Each slot's first
statement was a modal file dialog, which made the writing untestable and
unreusable, so it now lives in methods that take a destination.

Two claims get checked rather than asserted in a commit message. The saved log
is **the one that was sent**: the test writes a sample CSV that contains
`gnss_lat`/`gnss_lon`, previews, saves, and fails if either appears in the
output — replacing the writer with a copy of the source file is caught. And the
saved configuration is **genuinely re-uploadable**: it is loaded back through a
separate `VescInterface`, and a parameter value has to survive the trip.

Fixing that second test found a real defect in the first version of the CSV
writer: it emitted `name:name::2:0:0` and threw away the label and unit the
payload had deliberately kept, so "Speed ESC (km/h)" came back as `kmh_vesc`.

**The `mainWindow*` checks** — the real window, built as the application builds
it. `MainWindow::reloadPages` registers ~40 pages in one function and keeps the
navigation list and the stacked widget in step **by convention only**; nothing
in the code enforces it, and `on_pageList_currentRowChanged` does
`setCurrentIndex(currentRow)`, so any drift opens the wrong screen for every
page below it. Also that `openPage` — what `--showPage` uses — reaches the page
it names, that an unknown name moves nothing, and that the custom-config rows
are hidden with no board attached.

These run **last**, and the window is created once and never destroyed. Both
are deliberate: MainWindow's timer runs the startup checks, which end in
`Utility::checkVersion` and a live network request, so nothing may turn the
event loop after they run; and constructing three windows in one process
segfaulted on teardown, because deletions queued through `deleteLater` with no
loop to run them were executed against objects whose owners had gone.

**The `insights*` checks** — the flows whose breakage would be silent: the
default provider is the one that keeps data on this machine, a remote endpoint
does not claim otherwise, a keyless provider does not invite a key, and neither
button is pressable with no controller attached.

## Both themes

`run.sh` runs the binary twice: once dark, once with `--light`.

The light run does **not** repeat the structure snapshots. Those record names,
classes and text, none of which the theme changes, so a second set of baselines
would be a copy of the first. What the theme does change is which files are
loaded — `Utility::getThemePath` sends every icon lookup to
`res/+theme_light` — and which colour names exist, because the two palettes are
independent literal lists in `appstyle.cpp` and can drift apart. So the light
run executes only the checks whose outcome the theme can alter.

That tier pays for itself: deleting `res/+theme_light/icons/motor.png` from
`res.qrc` leaves the dark run fully green and fails the light one.

## Updating baselines

```sh
./update-baselines.sh
```

Run it deliberately, after a change you meant to make, and commit the diff with
that change — the diff *is* the record of what the change did to the interface.
Never run it to turn a red suite green without reading what moved.

## Mutation results

A suite that cannot fail is worse than no suite. These were each applied, run,
and reverted:

| mutation | result |
|---|---|
| rename `generalTab` in `pageapppas.ui` | **caught** — `PageAppPas` structure, naming the line |
| delete the `addParamSubgroup` call in `pageapppas.cpp` | **caught twice** — structure *and* `paramPagesAreNotEmpty` |
| enable Analyse unconditionally in `pagetuninginsights.cpp` | **caught twice** — structure *and* `insightsButtonsNeedAController` |
| delete both `updateParamDouble` calls in `parameditdouble.cpp` | **caught** — `editorWritesThroughToConfig` |
| remove a colour the pages request from `appstyle.cpp` | **caught** — `noMissingIconsOrColours` |
| revert the welcome heading to the upstream name | **caught** — the `branding` stage of `tests/check.sh` |
| add a nav row with no page behind it | **caught** — `mainWindowNavAndStackStayInStep` |
| delete a light-theme icon from `res.qrc` | **caught by the light run only** — dark stayed green |
| save the source log instead of the filtered one | **caught** — `insightsSavesWhatWasSent`, gnss reappeared |
| write the config in a form that cannot be loaded | **caught** — `insightsSavedConfigLoadsBackIn` |
| set a label from `currentMSecsSinceEpoch()` on a timer | **caught** — `snapshotsAreStable` |
| fill a combo from the host (serial ports) | **not a mutation** — found while generating baselines, see below |

Three of those were *not* caught when first written, and the reasons are worth
keeping:

- **The round-trip suite was vacuous.** `paramEditorKeepsItsValue` only read
  the spin box's own value back, so severing both `updateParamDouble` calls —
  which disconnects every parameter editor from the configuration that gets
  written to a controller — left the suite fully green.
  `editorWritesThroughToConfig` now diffs `ConfigParams` and asserts the one
  parameter that changed is the editor's own `name()`.
- **The message capture collected nothing.** `QTest::qExec` installs its own
  message handler, replacing the one installed in `main()`. The colour warning
  was emitted 42 times and the test still passed. The handler is now installed
  inside the test and chains to whatever QTest put there.
- **The branding test could not see the string that motivated it.** The welcome
  heading lives in `pagewelcome.ui`, and that page is QML-backed and excluded
  from the suite, so a widget test cannot reach it. That check is a grep over
  the sources in `tests/check.sh` instead.

## Two traps in the harness itself

**`processEvents` advances no wall-clock time.** It returns at once when the
queue is empty, so a loop of it — however many iterations — never lets a timer
fire. The first stability test looped it twenty-five times, called that "long
enough for a one-second timer", and did not notice a label being set to
`QDateTime::currentMSecsSinceEpoch()` every 500 ms. `UiHarness::settleWithTimers`
uses `QTest::qWait`, which waits.

**Some combos are filled from the host.** `serialPortBox` and `victronPortBox`
list this machine's serial ports, so the first baselines for `PageEspProg` and
`PageExperiments` contained `ttyACM0` and would have failed on any other
machine, or on this one with the board unplugged. They are recorded as
`"(filled from the host; not recorded)"` — a note rather than an omission, so
the control is still known to exist and the reason is visible in the baseline.

## Hermeticity

`UiHarness::pinEnvironment()` runs before the `QApplication` exists — which is
why `tst_ui.cpp` has its own `main()` rather than using `QTEST_MAIN`, since that
macro constructs the application first.

It points all four XDG roots and `HOME` at a fresh temporary directory, and pins
`LC_ALL`/`LANG`/`LC_NUMERIC` (the decimal separator is part of the rendered text
of every spin box), `TZ`, and the three scaling variables.

`XDG_DATA_HOME` matters as much as `XDG_CONFIG_HOME`: `Utility::configPath`
registers `$AppData/res_config.rcc` over the compiled-in parameter XML when that
file exists, so a developer who has ever downloaded a config archive would
otherwise be testing different parameters from everyone else. That is not
hypothetical — a stale copy of that file is what made the CLI refuse to connect
during this work, reporting "No Supported Firmwares".

The application's colour table, fonts and style come from `appstyle.cpp`, which
was extracted from `main.cpp` for this. The colour table is not cosmetic:
`Utility::getAppQColor` returns red for a name it does not know and logs a line
per miss, and a single page makes dozens of lookups.

## Pages covered, and the ones that are not

25 of 40 pages are covered. The rest are excluded for stated reasons rather than
silently skipped.

**Covered (25)** — constructor only calls `setupUi`, `setVesc` is null-guarded,
no filesystem or network: the `pageapp*` family, `pagebldc`, `pagedc`, `pagefoc`,
`pagegpd`, `pagecontrollers`, `pagemotor`, `pagemotorinfo`, `pagemotorsettings`,
`pagedataanalysis`, `pageterminal`, `pagedebugprint`, `pagetuninginsights`,
`pagebms`, `pagecananalyzer`, `pagesetupcalculators`, `pagertdata`,
`pagesampleddata`.

**Now covered, once the harness became hermetic** — `pagefirmware`,
`pageswdprog`, `pageespprog`, `pagevescpackage`, `pagelisp`,
`pagecustomconfig` (with `setConfNum(0)`, as MainWindow does) and
`pageexperiments`. Their constructors read `QSettings` last-used paths and
recent-file lists, which a temporary `XDG_CONFIG_HOME` makes absent, and that
absence is the deterministic state. 32 of 40 pages.

**Still not covered** — these read the host in a way no environment variable
fixes:

| page | why |
|---|---|
| `pageconnection` | `pageconnection.cpp:78` binds **UDP 65109** with `ShareAddress`, and the handler appends to `tcpDetectBox` on any VESC Tool broadcast on the LAN. No environment variable fixes this; the page would need the bind moved out of its constructor, which is the right change anyway — a page constructor should not open a socket. `setVesc` also enumerates host serial ports |
| `pagescripting` | reads `QSettings` recent files **and** hosts a `QQuickWidget`, so it is in the GL group below |
| `pagedisplaytool` | two `QFontComboBox`es enumerate the system font database; needs fontconfig pinned to the bundled faces before any snapshot of it is stable |

**Not covered, needing OpenGL** — `offscreen` reports no GL capability:

| page | why |
|---|---|
| `pagemotorcomparison`, `pagewelcome` | a `QQuickWidget` is created by `setupUi`, so it cannot be avoided by skipping `setVesc` |
| `pageappimu`, `pageimu`, `pageloganalysis` | embed `Vesc3DView`, a `QOpenGLWidget` |

These need a real GL context; the intended approach is a second tier under
`xvfb-run` with `QT_QPA_PLATFORM=xcb` and `LIBGL_ALWAYS_SOFTWARE=1`, rather than
pretending `offscreen` is enough. `pageloganalysis` additionally fetches map
tiles over HTTP, which has to be blocked first.

Eight pages' `setVesc` dereference `mVesc` with no null guard (`pageconnection`,
`pageespprog`, `pageexperiments`, `pagelisp`, `pagemotorcomparison`,
`pagescripting`, `pageswdprog`, `pagewelcome`), so the harness always passes a
real `VescInterface`. That is a latent robustness bug in those pages, recorded
here rather than fixed.
