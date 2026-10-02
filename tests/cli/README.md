# Command-line tests

```sh
./run.sh            # or: ./tests/check.sh
```

Ten checks, as a process rather than as widget code, because these are paths
through `main.cpp` that no widget test can reach. No board and no display: the
screenshot path runs offscreen, which is the whole reason `--offscreen` had to
start working on the GUI branch.

Hermetic the same way the widget suite is — every XDG root points at a
temporary directory, and `intro_done` is seeded so MainWindow's startup checks
do not open the introduction wizard modally and hang.

## What is checked

- **`--insightsPrintPrompt`** writes the instructions, so they can be edited
  and handed back with `--insightsPromptFile`.
- **`--insightsOffline --dryRun`** produces valid JSON from a sample log that
  deliberately contains `gnss_lat`/`gnss_lon`, and neither the column names nor
  the coordinates appear in it, while the permitted columns do. This is the
  check a user can run for themselves, and replacing the allowlist with a
  narrow `gnss_lat` blacklist fails it.
- **The refusals**: `--insightsOffline` without `--dryRun`, without a log,
  `--insightsMaxTokens 0`, and an unknown provider. Each must say why and must
  not exit 0.
- **`--screenshot`** renders offscreen at the requested size, and a size below
  the window's minimum is *reported* rather than silently changed.
- **`--screenshotClick`** with a name that does not exist fails, says which
  control it could not find, and writes no file.
- **`--vescTcp` to a closed port** fails within 30 s and says why, rather than
  hanging or exiting 0.

## Two bugs these found

`--insightsProvider` was not validated on the offline path, so a typo in the
name was accepted in silence and the run looked as though it had used the
provider asked for. It is now checked when the argument is parsed.

`--screenshotSize` smaller than the window's minimum silently produced a
different size. The window's layouts will not shrink past about 1197x838, so
the flag can enlarge but not shrink; it now says so on stderr.

## A trap worth remembering

The first version of this suite mixed "capture stdout" with "grep stderr" and
bare `$?`, and reported a failure for `--screenshotClick` when the program was
behaving correctly — exit 195, the right message, no file written. Both streams
now go to files and `saw()` greps both.
