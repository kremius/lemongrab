TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp

LIBS += -lgloox -lpthread

QMAKE_CXXFLAGS += -std=c++11

OTHER_FILES += \
    update.sh \
    README.md \
    Makefile
