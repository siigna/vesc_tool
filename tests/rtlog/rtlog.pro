# Ride logging, end to end, against the firmware's own protocol code.
#
#   cd tests/rtlog && qmake && make -j8 && ./tst_rtlog
#
# Links the application through app.pri, because VescInterface is most of the
# program. Needs tests/vescsim from the firmware tree; see tst_rtlog.cpp.

QT       += testlib
CONFIG   += c++11 testcase
CONFIG   -= app_bundle
CONFIG   += exclude_fw

TARGET = tst_rtlog
TEMPLATE = app

TOP = $$PWD/../..

include($$TOP/app.pri)

INCLUDEPATH = $$TOP $$INCLUDEPATH

OBJECTS_DIR = $$PWD/obj
MOC_DIR     = $$PWD/obj
RCC_DIR     = $$PWD/obj
UI_DIR      = $$PWD/obj

SOURCES += $$PWD/tst_rtlog.cpp
