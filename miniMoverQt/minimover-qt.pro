QT += core gui qml quick quickcontrols2 dbus
CONFIG += c++17 release
TARGET = minimover-qt
TEMPLATE = app
INCLUDEPATH += ../miniMoverLib
HEADERS += src/backend.h src/worker.h src/theme.h src/translator.h src/filepicker.h
SOURCES += src/main.cpp src/backend.cpp src/worker.cpp src/theme.cpp src/translator.cpp src/filepicker.cpp
include(library.pri)
RESOURCES += src/resources.qrc
