@echo off
setlocal

:: Hardcoded shader paths and compile settings
set "DXC_PATH=%DX12_DXC_X64%"
echo DXC_PATH: %DXC_PATH%

set "DXC_EXE=%DXC_PATH%\dxc.exe"
echo DXC_EXE: %DXC_EXE%

:: Get and display the batch file path
set "BATCH_FILE_PATH=%~dp0"
echo Batch file path: %BATCH_FILE_PATH%


