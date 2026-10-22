# RESQ - Rescue and Emergency Support System
# Build with:  make          (application)
#              make test     (automated tests)
#              make clean
# Works with GNU make + GCC/Clang on Linux, macOS, MSYS2/MinGW (Windows)
# and Termux (Android: make CC=clang CXX=clang++).

CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -std=c11 -Wall -Wextra -pedantic -O2
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2
CPPFLAGS += -Iinclude
LDFLAGS  ?=

ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

BUILD   := build
APP     := resq$(EXE)
TESTBIN := resq_tests$(EXE)

CORE_CPP := src/types.cpp src/util.cpp src/priority.cpp src/emergency.cpp \
            src/resource.cpp src/emergency_manager.cpp src/resource_manager.cpp \
            src/dispatcher.cpp src/file_manager.cpp src/analytics.cpp \
            src/demo_data.cpp src/console.cpp
APP_CPP  := src/resq_app.cpp src/main.cpp
C_SRC    := src/resq_c.c

CORE_OBJ := $(patsubst src/%.cpp,$(BUILD)/%.o,$(CORE_CPP)) $(BUILD)/resq_c.o
APP_OBJ  := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_CPP))
TEST_OBJ := $(BUILD)/test_resq.o

HEADER_ORDER := types util priority emergency resource emergency_manager resource_manager \
                dispatcher file_manager analytics demo_data console resq_app
SOURCE_ORDER := types util priority emergency resource emergency_manager resource_manager \
                dispatcher file_manager analytics demo_data console resq_app main

.PHONY: all test clean single

all: $(APP)

$(APP): $(CORE_OBJ) $(APP_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TESTBIN): $(CORE_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TESTBIN)
	./$(TESTBIN)

$(BUILD)/%.o: src/%.cpp $(wildcard include/*.h) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/resq_c.o: src/resq_c.c include/resq_c.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/test_resq.o: tests/test_resq.cpp $(wildcard include/*.h) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

# Regenerates resq_single.cpp: every header and source combined into one
# file, for IDEs and online compilers that build only a single file.
single:
	{ sed -e '/#include "resq_c.h"/{r include/resq_c.h' -e 'd;}' src/resq_c.c; \
	  for h in $(HEADER_ORDER); do grep -v '^#include "' include/$$h.h; done; \
	  for s in $(SOURCE_ORDER); do grep -v '^#include "' src/$$s.cpp; done; \
	} > resq_single.cpp

clean:
	rm -rf $(BUILD) resq resq.exe resq_tests resq_tests.exe
