# Tests for Tuning Insights. The first test harness in this repository.
#
#   nix-shell -p qt5.qtbase qt5.qtserialport gnumake gcc \
#       --run 'cd tests/tuning && qmake && make && ./tst_tuning'
#
# GUILESS and no network: everything under test is the pure payload layer and
# the provider seam, which is why both were kept free of I/O. Nothing here
# touches a board, a display or a provider.

# No gui, no widgets, no quick. The payload layer and the provider seam do not
# reach ConfigParams, which is what keeps this binary small enough to be worth
# having -- see the note in tuninginsights.h.
QT       += core network testlib
QT       -= gui

CONFIG   += c++11 console testcase
CONFIG   -= app_bundle

TARGET = tst_tuning
TEMPLATE = app

TOP = ../..

INCLUDEPATH += $$TOP

SOURCES += tst_tuning.cpp \
           $$TOP/tuninginsights.cpp \
           $$TOP/insightsprovider.cpp \
           $$TOP/tuningclient.cpp

HEADERS += $$TOP/tuninginsights.h \
           $$TOP/insightsprovider.h \
           $$TOP/tuningclient.h
