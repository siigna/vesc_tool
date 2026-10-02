# Widget tests

```sh
cd tests/ui && qmake && make -j8 && ./run.sh      # or: ./tests/check.sh
```

No board, no display server, no network. 38 pages and 24 behaviour checks,
about 24 seconds for the offscreen tier -- 7 s of that is `snapshotsAreStable`
waiting out constructor timers in real time, and 5 s is building all 38 pages
twice. Nothing here needs a window manager or a synthetic mouse: a page is a
`QWidget` with a one-argument constructor, and the parameter XML is compiled
into the binary. The config round-trip and serialization checks, which are the
ones protecting backwards compatibility, take 4 ms together.

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

**`editor{Int,Bool,Enum,Bitfield,String}WritesThroughToConfig`** — the same
claim for the other five editor types, which were untested until they were not:
each has its own `updateParam*` call site, so a passing double proved nothing
about them. Each test walks the parameter pages, takes the first editor of its
type that can actually be moved, and asserts that **exactly one** parameter
changed, that it is the one the editor is named for, and that it holds the value
the editor was set to. "Exactly one" is the part that is easy to leave out: an
editor that writes the right name and also disturbs a neighbour would otherwise
pass.

Three traps are handled rather than stepped around, each because it would make
a test pass without testing anything. `ParamEditInt` keeps both spin boxes
alive and connected and `editAsPercentage` decides which one writes, so the test
drives whichever is visible. `ParamEditBitfield` connects to
`QCheckBox::clicked`, which `setChecked` does not emit, so the test calls
`click()`. And several editors write their starting value back through
`setConfig()`, so the before-snapshot is taken after the page has settled.

**`configSurvivesBinaryRoundTrip` / `configSurvivesXmlRoundTrip`** — the
config survives the trip to a controller and back, and to a saved file and
back. Perturbs every parameter that has room to move, clears all of them,
round-trips, and compares. Serialization is a flat stream with no field names,
so one parameter at the wrong width shifts every parameter after it with no
error anywhere; this is what notices. Doubles are snapped onto the wire's own
grid first so the expectation is exact — a tolerance wide enough to absorb
quantization absorbs real bugs too. Perturbation stays inside both the declared
range and what the tx type can carry, because `vbAppendDouble16` casts to
`qint16` *after* rounding, and that overflow wraps in a way that looks exactly
like the bug being hunted.

Two things worth knowing, neither a bug. `getXML` writes doubles through
`QString::number`, six significant digits, so a saved file is lossier than the
wire — that is the one tolerance in either test that is not the wire's own grid,
and when a saved config is compared against a controller, differences in the
last digits are the file format rather than drift. And bitfields go out through
`vbAppendInt8`, so a value with bit 7 set returns negative: the bit pattern
survives, the number does not. No shipped bitfield can reach it, since all four
label their top two bits "Unused" and the editor hides them.

**`configSignatureIsPinned`** — the signature is a CRC over every parameter's
name, type, tx type and enum labels, and `confgenerator_deserialize_appconf`
rejects the **whole blob** on a mismatch, in both directions, with "Invalid
signature" as the only clue. So an edit to the parameter XML that is not
followed by regenerating the firmware's `confgenerator.h` breaks config
transfer completely. The pair is pinned here, and additionally compared against
the firmware's own header when that tree is next door — `BLDC_DIR`, or
`../../../bldc` — and skipped with a note when it is not, since this suite has
to pass in a clone of `vesc_tool` alone. When it fails because the XML changed
on purpose: regenerate `confgenerator.h`, flash it, and update the pin in the
same commit.

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

**`connectionTcpButtonsAreNotSwapped`** — on the TCP tab, connect is the
*rightmost* of the two icons and disconnect sits to its left, which is the
opposite of what most people assume; driving that page by mouse coordinates I
clicked disconnect three times in a row while wondering why nothing connected.
Checked by grid column, not pixel position: those buttons live on a tab that is
not current, so nothing in that subtree has resolved geometry and every child
reports the same x — a position check there compares two equal numbers and
passes whichever way round they are.

**`connectionDoesNotBindUntilAsked`** — building the page must not open a
socket, verified by binding UDP 65109 exclusively afterwards, and
`startDetection()` must then open it, so the move did not quietly disable the
feature.

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

## Three tiers

`run.sh` runs the binary three times.

| tier | platform | what runs |
|---|---|---|
| default | `offscreen` | everything except the GL pages (72 checks) |
| `--light` | `offscreen` | the checks the theme can change (4) |
| `--gl` | `xcb` under `xvfb`, `LIBGL_ALWAYS_SOFTWARE=1` | the 6 pages that need a context (8) |

Naming a slot runs it in the default tier only, so a slot that lives in one of
the other two needs its flag — `./run.sh --gl glPagesRender`,
`./run.sh --light noMissingIconsOrColours` — or it skips, and a skip reads as a
pass. `tests/mutate.py` passes the flag for any mutation that declares
`tier:`.

Three pages host a `QQuickWidget`, created by `setupUi` so it cannot be avoided
by skipping `setVesc`, and three embed `Vesc3DView`, a `QOpenGLWidget`. The
offscreen platform reports no GL capability, so they get a real X server and
Mesa's software rasteriser. Under the default tier they **skip** with a reason
rather than pass: offscreen still builds most of their widget trees, so they
would compare against baselines taken with a context and report a confusing
mismatch for one page and a pass for the rest.

The GL tier is skipped, not failed, when `xvfb-run` is unavailable — the rest
of the suite is still worth running.

**It was skipped on every run for the life of this suite.** `nix develop` had
no `devShells.default`, so it fell back to the package's build environment,
which carries the Qt modules and nothing else. Every GL row reported `SKIP`,
`run.sh` printed its one-line notice, and six pages went untested while the
table above said they did not. There is a `devShells.default` now with
`xvfb-run`, `mesa` and `python3` in it, and mutation `13` renames a tab label
on `PageImu` specifically so that something fails if this ever silently stops
running again. The skip notice is also louder than it was.

**Two things had to be fixed before those snapshots meant anything.** The Qt
QML import paths were not set, so the engine reported
`module "QtQuick.Controls" is not installed` and loaded no scene; `qtenv.sh`
now assembles `QML2_IMPORT_PATH` from every package that ships a `qml/`
directory. And `Vedder.vesc.utility` — the program's *own* QML module — was
registered inside `main.cpp`, which a test binary never runs, so those
registrations moved to `appregister.cpp` and both the application and the tests
call them. Until both were done, `PageWelcome`'s snapshot recorded an empty
view and looked perfectly healthy.

## Both themes

`run.sh` runs the binary twice: once dark, once with `--light`.

The light run does **not** repeat the structure snapshots. Those record names,
classes and text, none of which the theme changes, so a second set of baselines
would be a copy of the first. What the theme does change is which files are
loaded — `Utility::getThemePath` sends every icon lookup to
`res/+theme_light` — and which colour names exist, because the two palettes are
independent literal lists in `appstyle.cpp` and can drift apart. So the light
run executes only the checks whose outcome the theme can alter.

That tier pays for itself: deleting `res/+theme_light/icons/Upload-96.png` from
`res.qrc` leaves the dark run fully green and fails the light one — mutation
`15`, and `Upload-96.png` is the read button on every parameter editor, so
every page asks for it.

**The colour half of that claim was weaker than it reads**, which trying to
write a mutation for it is what showed. `getAppQColor` answers an unknown name
with red *and a debug message*, which is what the check watches for — but
`Utility::mAppColors` is a single static map seeded with a built-in default for
most names, and `setAppQColor` overwrites entries in it. So a colour dropped
from one palette is not an unknown name: the lookup finds the seed value, logs
nothing, and the page draws a dark-theme grey in light mode. Removing
`disabledText` from the light palette was not caught by anything.

`paletteCoversBothThemes` covers that gap by comparing the two sets of names
in `appstyle.cpp` directly. It reads the source file, which is crude, and the
alternative was no coverage: there is no API to enumerate what a palette
defined, and once `initColors` has run a seed default is indistinguishable from
a value a palette set deliberately.

It found one immediately. `vescGreen` was defined in the dark palette only, in
neither the light one nor the seed map, so in light mode it would have resolved
to red — and it turned out to be dead, set and never read, with only
`vescGreenMedium` and `vescGreenDark` actually used. Removed, which is what
makes the invariant true rather than merely asserted.

## Updating baselines

```sh
./update-baselines.sh
```

Run it deliberately, after a change you meant to make, and commit the diff with
that change — the diff *is* the record of what the change did to the interface.
Never run it to turn a red suite green without reading what moved.

## Mutation results

A suite that cannot fail is worse than no suite. The severings that prove each
check bites are no longer a thing to remember doing: they live in
`tests/mutations/`, one file per mutation, and `tests/mutate.py` applies them.

```sh
./tests/mutate.py              # all of them, about four minutes
./tests/mutate.py string       # just the ones whose name or test matches
./tests/mutate.py --list       # what is defined, and why each one matters
```

It refuses to start unless the tests it is about to break are green, cuts one
mutation at a time, rebuilds, runs only the test that should notice, and
restores every file afterwards -- including on Ctrl-C, verified by hash before
it exits. Three outcomes other than caught or not caught are reported as their
own thing rather than folded into either: a pattern that no longer matches, a
mutation that does not compile, and a test name that matched nothing. Each of
those would otherwise read as "not caught" and send somebody after a
non-existent hole. Currently 12 mutations, 12 caught.

Adding a check to the suite means adding the mutation that proves it bites. A
mutation may declare `occurrences:` when an editor writes through from more
than one place -- `ParamEditInt` has a plain box and a percentage box -- so
that all the sites are cut and the test cannot pass through a surviving one.

These were each applied, run, and reverted; the first group now lives in
`tests/mutations/`:

| mutation | result |
|---|---|
| rename `generalTab` in `pageapppas.ui` | **caught** — `PageAppPas` structure, naming the line |
| delete the `addParamSubgroup` call in `pageapppas.cpp` | **caught twice** — structure *and* `paramPagesAreNotEmpty` |
| enable Analyse unconditionally in `pagetuninginsights.cpp` | **caught twice** — structure *and* `insightsButtonsNeedAController` |
| delete both `updateParamDouble` calls in `parameditdouble.cpp` | **caught** — `editorWritesThroughToConfig` |
| sever the write-through in each of the other five editors | **caught** — one test each, none catching another's |
| truncate strings unconditionally again (`maxLen` 0) | **caught** — `editorStringWritesThroughToConfig` |
| serialize one int at the wrong width | **caught** — `configSurvivesBinaryRoundTrip` |
| skip the first parameter in the serialize order | **caught** — `configSurvivesBinaryRoundTrip` |
| make `deSerialize` a no-op | **caught** — proves the test clears values first |
| stop hashing the tx type into the signature | **caught** — `configSignatureIsPinned` |
| write every XML double as zero | **caught** — `configSurvivesXmlRoundTrip` |
| remove a colour the pages request from `appstyle.cpp` | **caught** — `noMissingIconsOrColours` |
| revert the welcome heading to the upstream name | **caught** — the `branding` stage of `tests/check.sh` |
| add a nav row with no page behind it | **caught** — `mainWindowNavAndStackStayInStep` |
| delete a light-theme icon from `res.qrc` | **caught by the light run only** — dark stayed green |
| rename a tab label on `PageImu` | **caught by the GL tier only** — and proves that tier runs |
| define a colour in one palette and not the other | **caught** — `paletteCoversBothThemes` |
| save the source log instead of the filtered one | **caught** — `insightsSavesWhatWasSent`, gnss reappeared |
| write the config in a form that cannot be loaded | **caught** — `insightsSavedConfigLoadsBackIn` |
| set a label from `currentMSecsSinceEpoch()` on a timer | **caught** — `snapshotsAreStable` |
| rename `accelPlot` on a GL page | **caught by the GL tier only** — the offscreen run skips it |
| swap the TCP connect/disconnect columns | **caught** — `connectionTcpButtonsAreNotSwapped` |
| bind UDP 65109 in `PageConnection`'s constructor again | **caught** — `connectionDoesNotBindUntilAsked` |
| fill a combo from the host (serial ports) | **not a mutation** — found while generating baselines, see below |

Four of those were *not* caught when first written, and the reasons are worth
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
- **Five of the six editor types were never driven at all.** The round-trip
  check above covered `ParamEditDouble` on one page, and each of the other five
  editors has its own `updateParam*` call site that can be severed on its own,
  so a passing double was no evidence about any of them. Driving the string
  editor for the first time found a real defect: `updateParamString` truncated
  to `maxLen`, every shipped string parameter has `maxLen` 0, and
  `truncate(0)` empties the string -- so editing a motor's brand or
  description had never worked. Everything else already read 0 as "no limit".
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

## A private network namespace

`run.sh` runs the binary under `unshare -rn` when it can — unprivileged, via a
user namespace — and brings loopback up inside it.

Two reasons. UDP 65109 is then free, so the check that `PageConnection` does
not bind it in its constructor actually runs instead of skipping past a copy of
the program the developer happens to have open. And no device broadcast from
the real network can reach that page and change its widget tree mid-run.

Loopback has to be brought up by hand in the namespace or the transport test
cannot reach its own stub server. Without `unshare`, everything still runs; the
bind check skips with its reason.

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
