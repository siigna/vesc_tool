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

**The `insights*` checks** — the flows whose breakage would be silent: the
default provider is the one that keeps data on this machine, a remote endpoint
does not claim otherwise, a keyless provider does not invite a key, and neither
button is pressable with no controller attached.

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

**Not yet covered, needing isolation** — these read the developer's state in
their constructors, so they are deterministic only once the preconditions are
handled:

| page | why |
|---|---|
| `pageconnection` | `pageconnection.cpp:78` binds **UDP 65109** with `ShareAddress`, and the handler appends to `tcpDetectBox` on any VESC Tool broadcast on the LAN. No environment variable fixes this; the page would need the bind moved out of its constructor, which is the right change anyway — a page constructor should not open a socket. `setVesc` also enumerates host serial ports |
| `pagefirmware`, `pageswdprog`, `pageespprog`, `pagevescpackage` | read `QSettings` last-used paths and register `$AppData/*.rcc` archives; a fresh `XDG_*` covers most of it, and these are the next ones to add |
| `pagelisp`, `pagescripting` | read `QSettings` recent-file arrays and **open the host user's files into editor tabs** |
| `pageexperiments` | `setVesc` dereferences unguarded and the page enumerates host serial ports |
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
