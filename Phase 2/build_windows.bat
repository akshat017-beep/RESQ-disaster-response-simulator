@echo off
setlocal
cd /d "%~dp0"
gcc -std=c11 -Wall -Wextra -pedantic -Iinclude -c src\c\victim_triage.c -o victim_triage.o
if errorlevel 1 goto failed
gcc -std=c11 -Wall -Wextra -pedantic -Iinclude -c src\c\data_structures.c -o data_structures.o
if errorlevel 1 goto failed
gcc -std=c11 -Wall -Wextra -pedantic -Iinclude -c src\c\graph_algorithms.c -o graph_algorithms.o
if errorlevel 1 goto failed
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude -c src\cpp\rescue_system.cpp -o rescue_system.o
if errorlevel 1 goto failed
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude -c src\cpp\main.cpp -o main.o
if errorlevel 1 goto failed
g++ victim_triage.o data_structures.o graph_algorithms.o rescue_system.o main.o -o resq.exe
if errorlevel 1 goto failed
echo Build completed successfully.
exit /b 0
:failed
echo Build failed. Read the compiler error above.
exit /b 1
