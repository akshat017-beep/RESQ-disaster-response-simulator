@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
title RESQ Phase-III Simulator
cls

echo.
echo  ================================================================
echo               RESQ PHASE-III -- DISASTER RESPONSE SIMULATOR
echo  ================================================================
echo.

rem ---- 1. Compiler check --------------------------------------------------
where gcc >nul 2>nul
if errorlevel 1 goto :compiler_error
where g++ >nul 2>nul
if errorlevel 1 goto :compiler_error

rem ---- 2. Existing process detection --------------------------------------
rem  If phase3.exe is already running, kill it so the linker can overwrite it.
tasklist /FI "IMAGENAME eq phase3.exe" 2>nul | find /I "phase3.exe" >nul
if not errorlevel 1 (
    echo  Detected a running instance of phase3.exe -- terminating it...
    taskkill /IM phase3.exe /F >nul 2>nul
    timeout /t 1 /nobreak >nul
)

rem ---- 3. Test-suite option -----------------------------------------------
set RUN_TESTS=N
echo  Do you want to run the test suite before launching? (Y/N)
set /p RUN_TESTS=  Choice: 
if /I "!RUN_TESTS!"=="Y" goto :run_tests_first

goto :build_main

rem ---- 4a. Test-suite path ------------------------------------------------
:run_tests_first
echo.
echo  ================================================================
echo   BUILDING TEST SUITE
echo  ================================================================
echo.

if not exist build mkdir build

echo  [1/5] Compiling C data-structure module...
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -c src\dsa.c -o build\dsa.o
if errorlevel 1 goto :build_error

echo  [2/5] Compiling C graph module...
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -c src\graph.c -o build\graph.o
if errorlevel 1 goto :build_error

echo  [3/5] Compiling C++ resource module...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src\ResourceModel.cpp -o build\ResourceModel.o
if errorlevel 1 goto :build_error

echo  [4/5] Compiling C++ system controller...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src\SystemController.cpp -o build\SystemController.o
if errorlevel 1 goto :build_error

echo  [5/5] Compiling test suite...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c tests\phase3_tests.cpp -o build\phase3_tests.o
if errorlevel 1 goto :build_error

echo.
echo  Linking test executable...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic build\dsa.o build\graph.o build\ResourceModel.o build\SystemController.o build\phase3_tests.o -o build\phase3_tests.exe
if errorlevel 1 goto :build_error

echo.
echo  ================================================================
echo   RUNNING TESTS
echo  ================================================================
echo.
build\phase3_tests.exe
if errorlevel 1 (
    echo.
    echo  ERROR: One or more tests FAILED. See output above.
    pause
    exit /b 1
)

echo.
echo  All tests PASSED. Proceeding to build main simulator...
echo.

rem Reuse the already-compiled .o files for the main executable
goto :link_main

rem ---- 4b. Normal build (no tests) ----------------------------------------
:build_main
echo.
echo  ================================================================
echo   BUILDING RESQ PHASE-III
echo  ================================================================
echo.

if not exist build mkdir build

echo  [1/5] Compiling C data-structure module...
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -c src\dsa.c -o build\dsa.o
if errorlevel 1 goto :build_error

echo  [2/5] Compiling C graph module...
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -c src\graph.c -o build\graph.o
if errorlevel 1 goto :build_error

echo  [3/5] Compiling C++ resource module...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src\ResourceModel.cpp -o build\ResourceModel.o
if errorlevel 1 goto :build_error

echo  [4/5] Compiling C++ system controller...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src\SystemController.cpp -o build\SystemController.o
if errorlevel 1 goto :build_error

echo  [5/5] Compiling application entry point...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src\main.cpp -o build\main.o
if errorlevel 1 goto :build_error

:link_main
rem  Build main.o if it was not compiled on the tests path above
if not exist build\main.o (
    echo  Compiling application entry point...
    g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src\main.cpp -o build\main.o
    if errorlevel 1 goto :build_error
)

echo.
echo  Linking RESQ Phase-III executable...
g++ -std=c++17 -Wall -Wextra -Werror -pedantic build\dsa.o build\graph.o build\ResourceModel.o build\SystemController.o build\main.o -o phase3.exe
if errorlevel 1 goto :build_error

rem ---- 5. Build validation ------------------------------------------------
if not exist phase3.exe (
    echo  ERROR: phase3.exe was not created after linking.
    pause
    exit /b 1
)

echo.
echo  ================================================================
echo   Build completed successfully.
echo   Launching RESQ Phase-III Simulator...
echo.
echo   Interactive menu (17 options + Exit):
echo     1  Run fixed earthquake scenario (Dehradun map, 7 victims)
echo     2  Show disaster-area map
echo     3  Plan route between two locations (Dijkstra)
echo     4  Register a new victim call
echo     5  Show dashboard (all victims and stats)
echo     6  Sort victims by priority -- Insertion Sort
echo     7  Sort victims by priority -- Quick Sort
echo     8  Search for victim by ID (Linear + Binary search)
echo     9  Dispatch next pending victim
echo    10  Complete an in-progress operation
echo    11  Transfer resolved victim to hospital
echo    12  Show operation history (newest first -- non-destructive)
echo    13  Show all operations
echo    14  List rescue units
echo    15  List hospitals
echo    16  List active disasters
echo    17  Validate system invariants
echo     0  Exit
echo  ================================================================
echo.

.\phase3.exe

echo.
echo  RESQ Phase-III has closed.
pause
exit /b 0

rem ---- Error handlers -----------------------------------------------------
:compiler_error
echo.
echo  ERROR: GCC/G++ compiler not found on PATH.
echo  Install MinGW-w64 (https://www.mingw-w64.org/) and add its
echo  bin\ folder to the Windows PATH environment variable, then
echo  re-run this launcher.
echo.
pause
exit /b 1

:build_error
echo.
echo  ERROR: Build failed.  Read the compiler error message above.
echo  Common causes:
echo    - Syntax or type error in a source file.
echo    - Missing include\ or src\ directory.
echo    - Incompatible GCC/G++ version (requires C11 / C++17 support).
echo.
pause
exit /b 1

