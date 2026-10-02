# ESCargot Tool

Configuration and script-upload tool for motor controllers. A source-only fork
of VESC® Tool, with Lua package support and an expanded PAS configuration.

**Not affiliated with, endorsed by, or certified by Mr. Benjamin Vedder.**
VESC® is his registered trademark; see [TRADEMARKS.md](TRADEMARKS.md).

**No binary releases are published from this repository**, which is what the
upstream guidance below asks of a fork. For an official binary, go to
[vesc-project.com](https://vesc-project.com/) — that is the only channel that
can tell you a release is genuinely theirs.

## What this fork adds

### Lua packages

`pkgLua` in `pkgdesc.qml` builds and installs a package whose script is Lua
rather than LispBM. The container format is unchanged, so the existing upload,
erase and REPL paths carry it.

Two bugs were worth the trouble of finding:

* The whole container was being embedded where only its body belongs.
  `lispUpload` prepends its own six-byte header, so embedding the container
  nested one inside another and the firmware reported "4 source bytes, 301
  imports".
* The size report divided by the QML flash block rather than the script limit,
  which read 186.5% for a script that fitted comfortably. It now reports
  against both the ESP32 and STM32 limits, since one tool talks to both.

### An expanded PAS configuration

A 7.02 configuration carrying the PAS parameters: torque sensor, proportional
power, limits, pedalling threshold and assist cadence floor, walk assist, and
closed-loop power. PAS values are added to the realtime data and the log.

`APPCONF_SIGNATURE` is a CRC32c over the parameter names, types and order, so
a mismatch makes the board reject the config with no clue why. `tests/confsig`
in the firmware repository checks the two against each other.

### Connection

Follows the board's reported serial rate instead of assuming 115200.

## Building

Unchanged from upstream; see the sections below.

## Code Contribution, Distribution and Trademark Usage

VESC is a registered trademark of Benjamin Vedder. Read the [trademark policies](https://vesc-project.com/trademark_policies) for more information.

The "official" binary release of VESC Tool is done via VESC Project only, as that gives users a way to verify that releases, that use the registered VESC trademark, originate from the VESC Project. It is not ok to host a binary release on a different channel and use the VESC trademark for that release.

It is ok to use the github fork function to make contributions to the code. That is because 1) it is the most convenient way to make contributions and 2) the forked repository states clearly that it is a fork and points back to the main repository where the original code can be found. Further, it is easy to see what the code changes are from the forked repository compared to the main repository via github, but that information is lost in a binary release.

Forks of VESC Tool on github are not encouraged to provide a binary release in the repository. That is because there is no way to tell the final binary apart from the official release once downloaded. Further, packaging the firmware, which has to be done as an additional step from a different repository, also cannot be verified whether it is done correctly.

**Forks without branding**  

Because the topic came up, here are some words about forking VESC Tool and removing the branding.  

If you make a fork of VESC Tool and remove all traces of the VESC trademark you are not breaking the trademark policies, but we still do not encourage that. The reason is that such forks are 1) confusing to users and 2) divert users away from the VESC Project itself and take away the opportunity to learn about it and to make donations if they choose to. For example, the majority of VESC donations today come from VESC Tool downloads.  

If you see a missing feature and you want to put in some work and make that feature available, we would appreciate if you contribute that back to the main VESC repositories. That way there is only one consistent and compatible release for everyone that is managed by the main authors of the VESC code who make the vast majority of the development. It also gives the main authors, who are the most familiar with the code, a chance to review features to make sure that they are as safe as possible and don't break other parts of the functionality.

## Add Your Hardware to the Upstream Binary Release

This section is upstream's, kept because it is still the route to getting
hardware supported officially. It is not about this fork, which publishes no
binaries.

If you have custom hardware and you want to add support for it in the official release of VESC® Tool, you can use the following steps:

1) Go to https://github.com/vedderb/bldc and use the github fork function.  
2) Make your changes, test them and make a pull request to the main repository.  
3) If the pull request gets accepted your hardware will become part of the next official release. It will show up in the binary beta typically after a few days and in the stable version the next time a stable release is made.

## Development

**Note:** These instructions build VESC Tool without the BLDC firmwares bundled.

### Linux

Make sure that the required dependencies are installed. There is some advice in the [build_lin](./build_lin) file's comments. If you have Nix installed see below.

```shell
qmake -config release "CONFIG += release_lin build_original exclude_fw"
make -j8
./build/lin/vesc_tool_6.06
```

### Nix

The most easy way to build and run VESC Tool is to just run the provided program:

```shell
nix run
```

This will rebuild the program from scratch on each invokation. To enter a build environment with the dependencies installed for building it manually with QMake, run

```shell
nix develop
```

Then follow the normal build instructions for Linux.

### Starting QT Creator in Nix

QT Creator allows you to easily build and run the project. It also allows you to edit the page UIs with it's graphical editor. To run it using Nix simply start QT Creator from a shell with the build dependencies:

```shell
nix develop
nix run nixpkgs#qtcreator
```

This makes sure that QT Creator has access to the required dependencies.
