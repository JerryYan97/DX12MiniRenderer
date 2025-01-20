@echo off
setlocal

:: %0: Shader Name. %1: 

set "DXC_PATH=%DX12_DXC_X64%"
echo DXC_PATH: %DXC_PATH%

set "DXC_EXE=%DXC_PATH%\dxc.exe"
echo DXC_EXE: %DXC_EXE%

:: Get and display the batch file path
set "BATCH_FILE_PATH=%~dp0"
echo Batch file path: %BATCH_FILE_PATH%

set "SHADER_COMPILE=%DXC_EXE% %BATCH_FILE_PATH%CustomRTShader.hlsl -T lib_6_3 -Fh %BATCH_FILE_PATH%CustomRTShader.fxh -Vn CustomRTShader" /enable_unbounded_descriptor_tables

echo Executing command: %SHADER_COMPILE%
call %SHADER_COMPILE%

endlocal