# Attribution

## Placeholder logo

The snail used as the ESCargot Tool logo and application icon is a
**placeholder**, pending a commissioned logo.

> Roman snail (*Helix pomatia*) by **Geierunited** — Own work, **CC BY-SA 3.0**,
> <https://commons.wikimedia.org/w/index.php?curid=95926>

**Changes made:** cropped to a square around the animal, scaled down, and
composited with the text "ESCargot TOOL" to produce the wordmark and the
vertical sidebar strip.

Because those are modifications, the derived image files are themselves
licensed **CC BY-SA 3.0**, the same as the original:

```
res/version/escargot_tool.png
res/version/escargot_v.svg
res/+theme_light/version/escargot_tool.png
res/+theme_light/version/escargot_v.svg
res/logo_vertical.png
res/+theme_light/logo_vertical.png
```

Note that CC BY-SA 3.0 is not the same licence as the GPL that covers this
program's source. These image files are aggregated with the program, not part
of it, and they keep their own licence and attribution — the same arrangement
as a bundled font. Replacing them with the commissioned logo removes this
distinction.

The attribution is also shown in the application, under Help → About, so it
reaches someone who has the binary and not the repository.

## Trademarks

VESC® is a registered trademark of Benjamin Vedder. ESCargot Tool is a fork of
VESC® Tool and is **not affiliated with or endorsed by** him or the VESC
project.

The VESC logo, the "V" mark and the tier artwork belong to that project and are
not used as this fork's identity. The upstream artwork remains in
`res/version/` because the files are part of the inherited history, but the
build no longer references it — see the comment in `res_original.qrc`.

Where the VESC name appears in this program it is a reference to the upstream
project, its firmware or its hardware, not a claim to the mark.

One deliberate exception: `QCoreApplication::setApplicationName` is still
`"VESC Tool"` (`appstyle.cpp`). That string is not displayed — it is what
QSettings and `QStandardPaths` resolve paths from, so changing it would strand
every existing user's saved settings and connection history. It needs a
migration, not a rename.
