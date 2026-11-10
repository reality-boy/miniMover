QT += core gui qml quick quickcontrols2 dbus testlib
CONFIG += c++17 testcase
TARGET = minimover_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h ../src/worker.h ../src/theme.h ../src/translator.h ../src/filepicker.h
SOURCES += minimover_tests.cpp ../src/backend.cpp ../src/worker.cpp ../src/theme.cpp ../src/translator.cpp ../src/filepicker.cpp
include(../library.pri)
RESOURCES += ../src/resources.qrc
QMAKE_LFLAGS += -Wl,--wrap=send
