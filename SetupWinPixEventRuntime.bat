@echo off
setlocal

set "VERSION=1.0.240308001"
set "PACKAGE_URL=https://api.nuget.org/v3-flatcontainer/winpixeventruntime/%VERSION%/winpixeventruntime.%VERSION%.nupkg"
set "INSTALL_DIR=%~dp0Dependencies\WinPixEventRuntime"
set "PACKAGE_FILE=%TEMP%\WinPixEventRuntime.%VERSION%.nupkg"

if exist "%INSTALL_DIR%\Include\WinPixEventRuntime\pix3.h" (
    if exist "%INSTALL_DIR%\bin\x64\WinPixEventRuntime.lib" (
        if exist "%INSTALL_DIR%\bin\x64\WinPixEventRuntime.dll" (
            echo WinPixEventRuntime %VERSION% is already installed.
            exit /b 0
        )
    )
)

if exist "%INSTALL_DIR%" rmdir /s /q "%INSTALL_DIR%"
mkdir "%INSTALL_DIR%" || exit /b 1

echo Downloading WinPixEventRuntime %VERSION%...
curl.exe -fL "%PACKAGE_URL%" -o "%PACKAGE_FILE%"
if errorlevel 1 (
    echo Failed to download WinPixEventRuntime.
    rmdir /s /q "%INSTALL_DIR%"
    exit /b 1
)

echo Extracting WinPixEventRuntime...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory($env:PACKAGE_FILE, $env:INSTALL_DIR)"
if errorlevel 1 (
    echo Failed to extract WinPixEventRuntime.
    del /q "%PACKAGE_FILE%"
    rmdir /s /q "%INSTALL_DIR%"
    exit /b 1
)

del /q "%PACKAGE_FILE%"
echo WinPixEventRuntime %VERSION% installed successfully.
