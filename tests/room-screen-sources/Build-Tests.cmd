@echo off
setlocal
if "%QT_ROOT%"=="" (
  echo Set QT_ROOT to a Qt MSVC installation first.
  exit /b 2
)
if "%JOM%"=="" set JOM=jom
cd /d "%~dp0"
set BUILD_DIR=.build
set EXTRA_CONFIG=
if "%~1"=="unsupported" set BUILD_DIR=.build-unsupported
if "%~1"=="unsupported" set EXTRA_CONFIG=CONFIG+=unsupported
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%
"%QT_ROOT%\bin\qmake.exe" ..\source_tests.pro CONFIG+=release CONFIG-=debug %EXTRA_CONFIG%
if errorlevel 1 exit /b 1
"%JOM%" /f Makefile.Release
if errorlevel 1 exit /b 1
set "PATH=%QT_ROOT%\bin;%PATH%"
release\source_tests.exe
exit /b %errorlevel%
