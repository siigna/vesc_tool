# Widget-level tests: structure snapshots, interaction, and config round-trip.
#
#   cd tests/ui && qmake && make -j8 && ./run.sh
#
# This links the whole application through app.pri rather than re-listing its
# sources, because a page cannot be constructed without most of the program
# behind it. main.cpp is absent from app.pri by design -- it defines main(),
# and this binary supplies its own so the environment can be pinned before the
# QApplication exists (QTEST_MAIN would construct it too early).

QT       += testlib
CONFIG   += c++11 testcase
CONFIG   -= app_bundle
CONFIG   += exclude_fw     # forced here, so a bare qmake never pulls firmware

TARGET = tst_ui
TEMPLATE = app

TOP = $$PWD/../..

include($$TOP/app.pri)

# Deliberately prepended, not appended. qmarkdowntextedit ships its own
# mainwindow.h and its .pri puts that directory on the include path, so
# `#include "mainwindow.h"` resolved to a vendored demo app's class and the
# compiler reported MainWindow as having no openPage. The application only
# avoids this because -I. happens to come first there.
INCLUDEPATH = $$TOP $$INCLUDEPATH

# So the suite can find its baselines no matter where it is run from.
DEFINES += TESTS_UI_DIR=\\\"$$PWD\\\"

# Keep generated files out of the source tree.
OBJECTS_DIR = $$PWD/obj
MOC_DIR     = $$PWD/obj
RCC_DIR     = $$PWD/obj
UI_DIR      = $$PWD/obj

SOURCES += $$PWD/uiharness.cpp \
           $$PWD/tst_ui.cpp

HEADERS += $$PWD/uiharness.h
