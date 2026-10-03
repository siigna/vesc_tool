# QML-level tests for the mobile UI.
#
#   cd tests/qml && qmake && make -j8 && ./run.sh
#
# The mobile application is 56 QML files and ~19.5k lines that the widget
# suite cannot reach at all: it builds QWidget pages, and mobile/main.qml is a
# different application entry point. Nothing here had any test before.
#
# Links the whole application through app.pri for two reasons: mobile/qml.qrc
# is where the QML lives, and the types the QML imports (Vedder.vesc.*) are
# registered from C++ by appregister.cpp. A bare qmltestrunner cannot do
# either, which is why this is a binary and not a runner invocation.

QT       += testlib quick qmltest
CONFIG   += c++11 testcase
CONFIG   -= app_bundle
CONFIG   += exclude_fw     # forced here, so a bare qmake never pulls firmware

TARGET = tst_qml
TEMPLATE = app

TOP = $$PWD/../..

include($$TOP/app.pri)

# Prepended for the same reason as in tests/ui/ui.pro: qmarkdowntextedit ships
# its own mainwindow.h and puts that directory on the include path.
INCLUDEPATH = $$TOP $$INCLUDEPATH

# uiharness.cpp resolves the widget suite's baselines against this. Nothing
# here reads a baseline, but the harness is compiled whole.
DEFINES += TESTS_UI_DIR=\\\"$$PWD/../ui\\\"

# Where quick_test_main_with_setup looks for tst_*.qml.
DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"

OBJECTS_DIR = $$PWD/obj
MOC_DIR     = $$PWD/obj
RCC_DIR     = $$PWD/obj
UI_DIR      = $$PWD/obj

# Reused rather than copied: the environment pinning and the suppressed
# VescInterface are exactly what the widget suite already needed.
SOURCES += $$PWD/../ui/uiharness.cpp \
           $$PWD/tst_qml.cpp

HEADERS += $$PWD/../ui/uiharness.h
