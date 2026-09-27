# WebView2 is optional at build time. No browser/runtime is bundled by qmake.
INCLUDEPATH += $$PWD
HEADERS += $$PWD/WebPage.h $$PWD/WebPageCapture.h
SOURCES += $$PWD/WebPage.cpp $$PWD/WebPageCapture.cpp
win32 {
    isEmpty(WEBVIEW2_SDK): WEBVIEW2_SDK = $$PWD/../../build/webview2-sdk
    exists($$WEBVIEW2_SDK/build/native/include/WebView2.h) {
        INCLUDEPATH += $$WEBVIEW2_SDK/build/native/include
        DEFINES += ROOM_WEBVIEW2
    } else {
        message(WebPage built without WebView2 SDK. Run tools/fetch-webview2.py to enable rendering.)
    }
    LIBS += -lole32 -luser32
}
