#-------------------------------------------------
#
# Project created by QtCreator 2018-02-03T17:04:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = NihilStudio
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS \
            UNICODE \
            _UNICODE

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h

FORMS += \
        mainwindow.ui

INCLUDEPATH += ../NihilStudioCore/NihilStudioCore
INCLUDEPATH += ../NihilStudioCore/NihilStudioCore/gslib

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../NihilStudioCore/release/ -lNihilStudioCore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../NihilStudioCore/debug/ -lNihilStudioCore

INCLUDEPATH += $$PWD/../NihilStudioCore/Debug
DEPENDPATH += $$PWD/../NihilStudioCore/Debug

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../NihilStudioCore/release/libNihilStudioCore.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../NihilStudioCore/debug/libNihilStudioCore.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../NihilStudioCore/release/NihilStudioCore.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../NihilStudioCore/debug/NihilStudioCore.lib

win32: LIBS += -lUser32
win32: LIBS += -lGdi32
win32: LIBS += -lD3d11
win32: LIBS += -ldxgi
