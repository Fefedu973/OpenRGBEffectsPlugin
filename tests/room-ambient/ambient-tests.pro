QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ambient_image_tests
isEmpty(OPENRGB_ROOM_ROOT): OPENRGB_ROOM_ROOT = $$PWD/../../../OpenRGB-Room
INCLUDEPATH += $$PWD/../.. $$PWD/../../Effects/Ambient $$PWD/../../Effects $$OPENRGB_ROOM_ROOT $$OPENRGB_ROOM_ROOT/RGBController $$OPENRGB_ROOM_ROOT/qt $$OPENRGB_ROOM_ROOT/dependencies/json
SOURCES += $$PWD/ambient_image_tests.cpp $$PWD/../../ColorUtils.cpp $$OPENRGB_ROOM_ROOT/qt/hsv.cpp
