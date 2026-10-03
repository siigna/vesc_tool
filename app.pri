# Copyright 2016 - 2023 Benjamin Vedder	benjamin@vedder.se
# Copyright Jeffrey M. Friesen
# Copyright Marcos Chaparro
# Copyright 2026 Stephen Bouche
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Everything that defines the application's content and dependencies, shared by
# the app and by the test binaries under tests/.
#
# The source and resource lists were moved here out of vesc_tool.pro, which is
# where git records them: 36 of these lines are Benjamin Vedder's, 8 Jeffrey M.
# Friesen's and one Marcos Chaparro's. What is new is the split itself, and
# every path gaining a $$PWD/ -- a bare relative path in a .pri resolves
# against the including project's directory, so without it the test binaries
# look for the sources in tests/ui/.
#
# Split out of vesc_tool.pro so a widget test can link the whole application
# without duplicating its source list. The boundary is forced rather than
# chosen: the QT += block below is conditional on the HAS_* defines above it,
# and VT_GIT_COMMIT runs git at configure time, so a test target that did not
# share all of this would compile against a different feature set than the app.
#
# Every path here is $$PWD-prefixed, and so is every include(). Inside an
# included .pri, qmake resolves a bare relative path against the INCLUDING
# project's directory, so without the prefix a build from tests/ui would go
# looking for tests/ui/mainwindow.cpp.
#
# main.cpp is deliberately absent: it defines main(), which QTest supplies.
#-------------------------------------------------
#
# Project created by QtCreator 2016-08-12T21:55:19
#
#-------------------------------------------------

# Version
VT_VERSION = 7.02
VT_INTRO_VERSION = 1
VT_CONFIG_VERSION = 4

# Set to 0 for stable versions and to test version number for development versions.
VT_IS_TEST_VERSION = 1

# GIT commit
VT_GIT_COMMIT = $$system(git rev-parse --short=8 HEAD)

VT_ANDROID_VERSION_ARMV7 = 221
VT_ANDROID_VERSION_ARM64 = 222
VT_ANDROID_VERSION_X86 = 223

VT_ANDROID_VERSION = $$VT_ANDROID_VERSION_X86

# Ubuntu 18.04 (should work on raspbian buster too)
# sudo apt install qml-module-qt-labs-folderlistmodel qml-module-qtquick-extras qml-module-qtquick-controls2 qt5-default libqt5quickcontrols2-5 qtquickcontrols2-5-dev qtcreator qtcreator-doc libqt5serialport5-dev build-essential qml-module-qt3d qt3d5-dev qtdeclarative5-dev qtconnectivity5-dev qtmultimedia5-dev qtpositioning5-dev qtpositioning5-dev libqt5gamepad5-dev qml-module-qt-labs-settings qml-module-qt-labs-platform libqt5svg5-dev

DEFINES += VT_VERSION=$$VT_VERSION
DEFINES += VT_INTRO_VERSION=$$VT_INTRO_VERSION
DEFINES += VT_CONFIG_VERSION=$$VT_CONFIG_VERSION
DEFINES += VT_IS_TEST_VERSION=$$VT_IS_TEST_VERSION
DEFINES += VT_GIT_COMMIT=$$VT_GIT_COMMIT
QT_LOGGING_RULES="qt.qml.connections=false"
#CONFIG += qtquickcompiler

CONFIG += c++11
CONFIG += resources_big
ios: {
    QMAKE_CXXFLAGS_DEBUG += -Wall
}

android: {
    QMAKE_LFLAGS += -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384
}

!win32-msvc*: { !android: {
    QMAKE_CXXFLAGS += -Wno-deprecated-copy
}}

# Build mobile GUI
#CONFIG += build_mobile

# Exclude built-in firmwares
CONFIG += exclude_fw

ios: {
    CONFIG    += build_mobile
    DEFINES   += QT_NO_PRINTER
}

# Debug build (e.g. F5 to reload QML files)
#DEFINES += DEBUG_BUILD

# If BLE disconnects on ubuntu after about 90 seconds the reason is most likely that the connection interval is incompatible. This can be fixed with:
# sudo bash -c 'echo 6 > /sys/kernel/debug/bluetooth/hci0/conn_min_interval'

# Clear old bluetooth devices
# sudo rm -rf /var/lib/bluetooth/*
# sudo service bluetooth restart

# Bluetooth available
DEFINES += HAS_BLUETOOTH

# CAN bus available
# Adding serialbus to Qt seems to break the serial port on static builds. TODO: Figure out why.
#DEFINES += HAS_CANBUS

# Positioning
!win32: {
    DEFINES += HAS_POS
}

!ios: {
    QT      += printsupport
!android: {
    # Serial port available
    DEFINES += HAS_SERIALPORT
    DEFINES += HAS_GAMEPAD
}
}

win32: {
    DEFINES += _USE_MATH_DEFINES
}

# https://stackoverflow.com/questions/61444320/what-are-the-configure-options-for-qt-that-enable-dead-keys-usage
unix: {
!ios: {
    QTPLUGIN += composeplatforminputcontextplugin
}
}

# Options
#CONFIG += build_original
#CONFIG += build_platinum
#CONFIG += build_gold
#CONFIG += build_silver
#CONFIG += build_bronze
#CONFIG += build_free

QT       += core gui
QT       += widgets
QT       += network
QT       += quick
QT       += quickcontrols2
QT       += quickwidgets
QT       += svg
QT       += gui-private

contains(DEFINES, HAS_SERIALPORT) {
    QT       += serialport
}

contains(DEFINES, HAS_CANBUS) {
    QT       += serialbus
}

contains(DEFINES, HAS_BLUETOOTH) {
    QT       += bluetooth
}

contains(DEFINES, HAS_POS) {
    QT       += positioning
}

contains(DEFINES, HAS_GAMEPAD) {
    QT       += gamepad
}

android: QT += androidextras

build_mobile {
    DEFINES += USE_MOBILE
}

SOURCES += \
    $$PWD/appregister.cpp \
    $$PWD/appstyle.cpp \
    $$PWD/bleuartdummy.cpp \
    $$PWD/codeloader.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/boardsetupwindow.cpp \
    $$PWD/packet.cpp \
    $$PWD/pollmanager.cpp \
    $$PWD/preferences.cpp \
    $$PWD/tcphub.cpp \
    $$PWD/udpserversimple.cpp \
    $$PWD/vbytearray.cpp \
    $$PWD/commands.cpp \
    $$PWD/configparams.cpp \
    $$PWD/tuninginsights.cpp \
    $$PWD/tuninginsightsconf.cpp \
    $$PWD/insightsprovider.cpp \
    $$PWD/tuningclient.cpp \
    $$PWD/configparam.cpp \
    $$PWD/vescinterface.cpp \
    $$PWD/parametereditor.cpp \
    $$PWD/digitalfiltering.cpp \
    $$PWD/setupwizardapp.cpp \
    $$PWD/setupwizardmotor.cpp \
    $$PWD/startupwizard.cpp \
    $$PWD/utility.cpp \
    $$PWD/tcpserversimple.cpp \
    $$PWD/hexfile.cpp

HEADERS  += $$PWD/appregister.h \
    $$PWD/appstyle.h \
    $$PWD/mainwindow.h \
    $$PWD/bleuartdummy.h \
    $$PWD/codeloader.h \
    $$PWD/boardsetupwindow.h \
    $$PWD/packet.h \
    $$PWD/pollmanager.h \
    $$PWD/preferences.h \
    $$PWD/tcphub.h \
    $$PWD/udpserversimple.h \
    $$PWD/vbytearray.h \
    $$PWD/commands.h \
    $$PWD/datatypes.h \
    $$PWD/configparams.h \
    $$PWD/tuninginsights.h \
    $$PWD/tuninginsightsconf.h \
    $$PWD/insightsprovider.h \
    $$PWD/tuningclient.h \
    $$PWD/configparam.h \
    $$PWD/vescinterface.h \
    $$PWD/parametereditor.h \
    $$PWD/digitalfiltering.h \
    $$PWD/setupwizardapp.h \
    $$PWD/setupwizardmotor.h \
    $$PWD/startupwizard.h \
    $$PWD/utility.h \
    $$PWD/tcpserversimple.h \
    $$PWD/hexfile.h

unix: {
!ios: {
    HEADERS += $$PWD/systemcommandexecutor.h
}
}

FORMS    += $$PWD/mainwindow.ui \
    $$PWD/boardsetupwindow.ui \
    $$PWD/parametereditor.ui \
    $$PWD/preferences.ui

contains(DEFINES, HAS_BLUETOOTH) {
    SOURCES += $$PWD/bleuart.cpp
    HEADERS += $$PWD/bleuart.h
}

include($$PWD/pages/pages.pri)
include($$PWD/widgets/widgets.pri)
include($$PWD/mobile/mobile.pri)
include($$PWD/map/map.pri)
include($$PWD/lzokay/lzokay.pri)
include($$PWD/heatshrink/heatshrink.pri)
include($$PWD/QCodeEditor/qcodeeditor.pri)
include($$PWD/esp32/esp32.pri)
include($$PWD/display_tool/display_tool.pri)
include($$PWD/qmarkdowntextedit/qmarkdowntextedit.pri)
include($$PWD/maddy/maddy.pri)
include($$PWD/minimp3/minimp3.pri)

RESOURCES += $$PWD/res.qrc \
    $$PWD/res_custom_module.qrc \
    $$PWD/res_lisp.qrc \
    $$PWD/res_qml.qrc
RESOURCES += $$PWD/res/config/res_config.qrc

RESOURCES += $$PWD/res_fw_bms.qrc

!exclude_fw {
    RESOURCES += $$PWD/res/firmwares/res_fw.qrc
}

build_original {
    RESOURCES += $$PWD/res_original.qrc
    DEFINES += VER_ORIGINAL
} else:build_platinum {
    RESOURCES += $$PWD/res_platinum.qrc
    DEFINES += VER_PLATINUM
} else:build_gold {
    RESOURCES += $$PWD/res_gold.qrc
    DEFINES += VER_GOLD
} else:build_silver {
    RESOURCES += $$PWD/res_silver.qrc
    DEFINES += VER_SILVER
} else:build_bronze {
    RESOURCES += $$PWD/res_bronze.qrc
    DEFINES += VER_BRONZE
} else:build_free {
    RESOURCES += $$PWD/res_free.qrc
    DEFINES += VER_FREE
} else {
    RESOURCES += $$PWD/res_neutral.qrc
    DEFINES += VER_NEUTRAL
}
