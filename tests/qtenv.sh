# Sourced by the suites that need to start a Qt program. Not executable on its
# own.
#
# Qt will not start without being told where its platform plugins are. In this
# nix setup QT_PLUGIN_PATH is empty, `qmake -query QT_INSTALL_PLUGINS` answers
# with the -dev output, which has no plugins directory at all, and the -dev and
# -bin outputs have different store hashes, so one path cannot be derived from
# the other by substitution. Match on the Qt version instead.
#
# Two directories are needed, from two different packages:
#   qtbase-<ver>-bin  platforms/      (offscreen, xcb)
#   qtsvg-<ver>-bin   imageformats/   (svg)
# Without the second, QPixmap(":/res/icon.svg") silently returns a null pixmap,
# which is how the application icon went missing without anyone noticing.

qt_env_setup() {
    if [ -n "${QT_PLUGIN_PATH:-}" ]; then
        return 0
    fi

    local ver dev cand
    ver=$(qmake -query QT_VERSION 2>/dev/null || true)
    dev=$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)

    for cand in "$dev" /nix/store/*-qtbase-"$ver"-bin/lib/qt-"$ver"/plugins; do
        if [ -d "$cand/platforms" ]; then
            export QT_PLUGIN_PATH="$cand"
            break
        fi
    done

    if [ -z "${QT_PLUGIN_PATH:-}" ]; then
        echo "cannot find Qt platform plugins for Qt $ver;" \
             "set QT_PLUGIN_PATH to the directory containing platforms/" >&2
        return 2
    fi

    for cand in /nix/store/*-qtsvg-"$ver"-bin/lib/qt-"$ver"/plugins; do
        if [ -d "$cand/imageformats" ]; then
            export QT_PLUGIN_PATH="$QT_PLUGIN_PATH:$cand"
            break
        fi
    done

    qt_qml_setup "$ver"

    return 0
}

# QML imports, for the pages that host a QQuickWidget. Without these the engine
# reports `module "QtQuick.Controls" is not installed` and the scene never
# loads, so a snapshot of such a page records an empty view and looks fine.
#
# Every module that provides qml/ contributes, across several packages, some of
# them -bin outputs and some not -- so this globs rather than assuming a shape.
qt_qml_setup() {
    local ver="$1" d path=""

    for d in /nix/store/*-qt*-"$ver"*/lib/qt-"$ver"/qml \
             /nix/store/*-qtpositioning-*/lib/qt-"$ver"/qml; do
        if [ -d "$d" ]; then
            case ":$path:" in
                *":$d:"*) ;;
                *) path="${path:+$path:}$d" ;;
            esac
        fi
    done

    if [ -n "$path" ]; then
        export QML2_IMPORT_PATH="${QML2_IMPORT_PATH:+$QML2_IMPORT_PATH:}$path"
    fi
}
