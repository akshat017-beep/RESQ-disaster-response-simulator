@echo off
setlocal
cd /d "%~dp0"
title RESQ Phase-II Simulator
cls
echo.
echo  ================================================================
echo                 RESQ SIMULATOR
echo  ================================================================
echo.
where gcc >nul 2>nul
if errorlevel 1 goto compiler_error
where g++ >nul 2>nul
if errorlevel 1 goto compiler_error
call build_windows.bat
if errorlevel 1 goto build_error
if not exist resq.exe goto build_error
echo.
echo  Launching RESQ...
echo.
resq.exe
echo.
echo  RESQ has closed.
pause
exit /b 0
:compiler_error
echo  ERROR: GCC/G++ was not found.
echo  Install MinGW-w64 and add its bin folder to your Windows PATH.
pause
exit /b 1
:build_error
echo.
echo  The build failed. Check the compiler messages above.
pause
exit /b 1
