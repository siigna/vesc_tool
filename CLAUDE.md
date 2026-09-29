# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Nix (recommended)
```shell
# Build and run directly
nix run

# Enter dev shell with Qt deps, then build manually
nix develop
qmake -config release "CONFIG += release_lin build_original exclude_fw"
make -j8
./build/lin/vesc_tool_<version>
```

### Linux (manual)
```shell
# Dev build without bundled firmware (fastest)
qmake -config release "CONFIG += release_lin build_original exclude_fw"
make -j8

# With firmware bundled
qmake -config release "CONFIG += release_lin build_original"
make -j8
```

Build outputs go to `build/lin/`, `build/win/`, `build/macos/`, or `build/android/` depending on `CONFIG` flags.

### Build variants
The `CONFIG` flag controls both platform and branding tier:
- Platform: `release_lin`, `release_win`, `release_macos`, `release_android`
- Branding: `build_original`, `build_platinum`, `build_gold`, `build_silver`, `build_bronze`, `build_free` (omit for neutral/unbranded)
- `exclude_fw` skips bundling firmware resources (much faster for dev builds)
- `build_mobile` enables QML mobile UI (`USE_MOBILE` define, uses `mobile/` UI instead of desktop)

## Architecture

### Core communication stack
- **`packet.cpp`** — low-level framing: encodes/decodes byte-delimited VESC packets
- **`commands.cpp`** — VESC protocol commands; serializes/deserializes all VESC packet types; exposed to QML via `Q_INVOKABLE`
- **`vescinterface.cpp`** — central singleton managing all transport backends (serial, BLE, TCP, UDP, CAN) and owning `Commands`, `ConfigParams` instances; the main object injected into QML context
- **`configparams.cpp` / `configparam.cpp`** — parameter model; each `ConfigParam` holds type, value, limits, XML metadata; `ConfigParams` is a named collection

### UI layers
Two parallel UI stacks exist:
1. **Desktop (Qt Widgets)**: `mainwindow.cpp` + `.ui`, `pages/page*.cpp`, `widgets/`
2. **Mobile (QML)**: `mobile/main.qml`, `mobile/*.qml`, driven by `mobile/qmlui.cpp`

Pages in `pages/` are Qt Widget panels (motor config, app config, realtime data, firmware update, etc.). Each page holds a reference to `VescInterface` passed at construction.

### Configuration system
Motor config (`mcConfig`), app config (`appConfig`), info config, and fw config are separate `ConfigParams` instances on `VescInterface`. Config XML definitions live in `res/config/`. Parameters are read/written to VESC hardware via `Commands`.

### Resource system
Multiple `.qrc` files control what's bundled:
- `res.qrc` — icons, fonts, images
- `res_config.qrc` — XML config definitions
- `res_lisp.qrc` — LispBM scripts
- `res_qml.qrc` — QML files for mobile/dynamic UI
- `res_fw_bms.qrc` — BMS firmware
- `res_fw.qrc` — main firmware (excluded in dev via `exclude_fw`)
- `res_original.qrc` / `res_platinum.qrc` / etc. — branding assets per tier

### Feature flags (compile-time defines)
- `HAS_SERIALPORT` — serial port support (desktop Linux/Windows)
- `HAS_BLUETOOTH` — BLE support; uses `bleuart.cpp`, else `bleuartdummy.cpp`
- `HAS_CANBUS` — CAN bus via Qt serialbus (disabled by default; breaks serial on static builds)
- `HAS_POS` — GPS positioning (non-Windows)
- `HAS_GAMEPAD` — gamepad input (non-mobile desktop)
- `USE_MOBILE` — switches to QML mobile UI
- `DEBUG_BUILD` — enables hot-reload of QML files

### Application framework (`application/`)
A scripting layer letting users create custom VESC Tool apps. `create_app <name>` scaffolds a C++ or QML app that links against the VESC Tool backend.

### Submodule libraries
Vendored in-tree: `QCodeEditor/`, `qmarkdowntextedit/`, `lzokay/`, `heatshrink/`, `maddy/`, `minimp3/`, `esp32/`. Each has its own `.pri` included from `vesc_tool.pro`.

## Contributions

PRs require signing the CLA (`cla_vesc_tool.md`) by acknowledging it in the PR body. Contributions targeting the official binary should go through `github.com/vedderb/bldc` for firmware or this repo for tool changes.
