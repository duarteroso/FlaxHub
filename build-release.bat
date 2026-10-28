@echo off
rem Builds a Release FlaxHub and installs it to build\windows. windeployqt copies the Qt DLLs next to the app.
rem Set QT_ROOT to the Qt kit folder (the one holding bin\ and lib\cmake\Qt6) if it is not the default.
setlocal

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
rem A fresh build folder each run, removed afterwards, so no stale state is reused.
set "BUILD_DIR=%TEMP%\flaxhub-build-%RANDOM%%RANDOM%"
set "OUT_DIR=%ROOT%\build\windows"

if not defined QT_ROOT set "QT_ROOT=C:\Qt\6.12.0\msvc2022_64"
if not exist "%QT_ROOT%\lib\cmake\Qt6\" (
    echo Qt not found. Set QT_ROOT to your Qt kit folder.
    exit /b 1
)

cmake -S "%ROOT%\flaxhub" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="%QT_ROOT%\lib\cmake\Qt6" -DBUILD_TESTING=OFF || goto :fail
cmake --build "%BUILD_DIR%" --config Release --parallel || goto :fail

if exist "%OUT_DIR%" rmdir /s /q "%OUT_DIR%"
cmake --install "%BUILD_DIR%" --config Release --prefix "%OUT_DIR%" || goto :fail

"%QT_ROOT%\bin\windeployqt.exe" --release --qmldir "%ROOT%\flaxhub" "%OUT_DIR%\bin\appFlaxHub.exe" || goto :fail

rmdir /s /q "%BUILD_DIR%"
echo Release build installed to "%OUT_DIR%"
exit /b 0

:fail
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
echo Build failed.
exit /b 1
