TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS += -pthread
LIBS += -pthread

SOURCES += main.c

HEADERS += includes/main.h
