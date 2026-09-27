QT += core gui widgets network
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = webpage_tests
isEmpty(OPENRGB_ROOM_ROOT): OPENRGB_ROOM_ROOT = $$PWD/../../../OpenRGB-Room
isEmpty(WEBVIEW2_SDK): WEBVIEW2_SDK = $$PWD/../../build/webview2-sdk
INCLUDEPATH += $$PWD/../../Effects/WebPage $$PWD/../../Effects $$PWD/../.. $$OPENRGB_ROOM_ROOT $$OPENRGB_ROOM_ROOT/RGBController $$OPENRGB_ROOM_ROOT/qt $$OPENRGB_ROOM_ROOT/dependencies/json $$WEBVIEW2_SDK/build/native/include
HEADERS += $$PWD/../../Effects/WebPage/WebPageCapture.h
SOURCES += $$PWD/webpage_tests.cpp $$PWD/../../Effects/WebPage/WebPageCapture.cpp $$PWD/../../ColorUtils.cpp $$OPENRGB_ROOM_ROOT/qt/hsv.cpp
win32 {
    DEFINES += ROOM_WEBVIEW2
    LIBS += -lole32 -luser32
}
