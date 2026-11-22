# =============================================================================
# RESQ Phase III -- Makefile
#
# Targets:
#   all        Build phase3.exe (default)
#   tests      Build and run build/phase3_tests.exe
#   run        Build and launch phase3.exe interactively
#   clean      Remove build/ directory and compiled executables
#
# Requirements: GCC (C11) and G++ (C++17), available on PATH.
# =============================================================================

CC      = gcc
CXX     = g++
CFLAGS  = -std=c11  -Wall -Wextra -Werror -pedantic -Iinclude
CXXFLAGS= -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude
BUILD   = build

# Object files shared by both main and test executables
CORE = $(BUILD)/dsa.o \
       $(BUILD)/graph.o \
       $(BUILD)/ResourceModel.o \
       $(BUILD)/SystemController.o

# ---- Top-level targets -------------------------------------------------------

.PHONY: all tests run clean

all: phase3.exe

tests: $(BUILD)/phase3_tests.exe
	$(BUILD)/phase3_tests.exe

run: phase3.exe
	.\phase3.exe

# ---- Executable linking ------------------------------------------------------

phase3.exe: $(CORE) $(BUILD)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/phase3_tests.exe: $(CORE) $(BUILD)/phase3_tests.o
	$(CXX) $(CXXFLAGS) $^ -o $@

# ---- Compilation rules -------------------------------------------------------

$(BUILD):
	@if not exist $(BUILD) mkdir $(BUILD)

$(BUILD)/dsa.o: src/dsa.c include/resq.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/graph.o: src/graph.c include/resq.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ResourceModel.o: src/ResourceModel.cpp include/resq.hpp include/resq.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/SystemController.o: src/SystemController.cpp include/resq.hpp include/resq.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/main.o: src/main.cpp include/resq.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/phase3_tests.o: tests/phase3_tests.cpp include/resq.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- Cleanup -----------------------------------------------------------------

clean:
	@if exist $(BUILD) rmdir /s /q $(BUILD)
	@if exist phase3.exe del /q phase3.exe
	@if exist phase3_tests.exe del /q phase3_tests.exe

