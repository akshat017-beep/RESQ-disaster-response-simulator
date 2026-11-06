# RESQ — Disaster Response Simulator (Phase II)

RESQ is a console-based C/C++ educational project that simulates parts of a disaster-response workflow, including victim management, data structures, graph algorithms, and rescue-operation management.

## Run on Windows

**Requirements:** GCC and G++ installed and available in your `PATH` (for example, through MinGW-w64).

### One-click launch

1. Download or clone this repository.
2. Open the project folder.
3. Double-click `run_resq.bat`, or open the folder in VS Code and run this in the integrated terminal:

   ```powershell
   .\run_resq.bat
   ```

The launcher builds the program and starts it. No administrator permissions should normally be required.

### Build and run separately

In the VS Code terminal, from the project root:

```powershell
.\build_windows.bat
.\resq.exe
```

## Project structure

```text
RESQ-Disaster-Response-Simulator/
├── include/
│   ├── resq_c.h
│   └── rescue_system.h
├── src/
│   ├── c/
│   │   ├── victim_triage.c
│   │   ├── data_structures.c
│   │   └── graph_algorithms.c
│   └── cpp/
│       ├── main.cpp
│       └── rescue_system.cpp
├── Makefile
├── build_windows.bat
├── run_resq.bat
├── README.md
└── .gitignore
```

## Team responsibilities

- **Vanya Rathi:** victim triage and victim management — `src/c/victim_triage.c`
- **Akshat Dobhal:** data structures — `src/c/data_structures.c`
- **Ayush Kandwal:** graph algorithms — `src/c/graph_algorithms.c`
- **Nitin Singh:** C++ classes, interactive entry point, scenario testing, and integration — `src/cpp/rescue_system.cpp`, `include/rescue_system.h`, and `src/cpp/main.cpp`

Build and launch scripts support the project; `.gitignore` excludes generated object files and executables from Git commits.

## Scope note

This is an educational console simulation, not a real emergency dispatch service. The C modules are compiled into the executable, but they are not all fully wired into the interactive C++ menu yet. In particular, do not interpret the graph algorithms as live route calculation in the menu.
