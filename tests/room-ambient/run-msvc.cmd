@echo off
setlocal
rem Run from an x64 MSVC developer prompt: run-msvc.cmd QT_ROOT [OPENRGB_ROOM_ROOT] [BUILD_DIR]
set "QT_ROOT=%~1"
if not exist "%QT_ROOT%\bin\qmake.exe" (echo Supply the Qt MSVC installation directory. & exit /b 2)
where cl >nul 2>nul
if errorlevel 1 (echo Run from an x64 MSVC developer prompt. & exit /b 2)
set "ENGINE=%~2"
if not defined ENGINE set "ENGINE=%~dp0..\..\..\OpenRGB-Room"
set "OUT=%~3"
if not defined OUT set "OUT=%TEMP%\openrgb-room-ambient-tests"
set "REPO=%~dp0..\.."
if not exist "%OUT%" mkdir "%OUT%"
set "PATH=%QT_ROOT%\bin;%PATH%"
pushd "%OUT%"
"%QT_ROOT%\bin\qmake.exe" "%~dp0ambient-tests.pro" "OPENRGB_ROOM_ROOT=%ENGINE%" CONFIG+=release
if errorlevel 1 goto failed
nmake /nologo
if errorlevel 1 goto failed
"%OUT%\release\ambient_image_tests.exe"
if errorlevel 1 goto failed
rem Compile the actual effect and generated UI too; do not instantiate a capturer.
"%QT_ROOT%\bin\uic.exe" "%REPO%\Effects\Ambient\Ambient.ui" -o ui_Ambient.h
if errorlevel 1 goto failed
cl /nologo /c /std:c++17 /EHsc /MD /utf-8 /permissive- /Zc:__cplusplus /DNOMINMAX /DQT_WIDGETS_LIB /DQT_GUI_LIB /DQT_CORE_LIB /I"%REPO%" /I"%REPO%\Effects" /I"%REPO%\Effects\Ambient" /I"%REPO%\ScreenCapturer" /I"%REPO%\ScreenCapturer\qt" /I"%REPO%\ScreenCapturer\windows" /I"%REPO%\Audio" /I"%ENGINE%" /I"%ENGINE%\RGBController" /I"%ENGINE%\qt" /I"%ENGINE%\dependencies\json" /I"%QT_ROOT%\include" /I"%QT_ROOT%\include\QtCore" /I"%QT_ROOT%\include\QtGui" /I"%QT_ROOT%\include\QtWidgets" /I"%OUT%" "%REPO%\Effects\Ambient\Ambient.cpp" /FoAmbient.obj
if errorlevel 1 goto failed
popd
exit /b 0
:failed
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
