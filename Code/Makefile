CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -std=c11 -Wall -Wextra -pedantic -O2
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2
CPPFLAGS += -Iinclude

CORE  := src/types.cpp src/emergency.cpp src/resource.cpp src/emergency_manager.cpp src/resource_manager.cpp \
         src/file_manager.cpp src/analytics.cpp src/console.cpp
ORDER_H := resq_c types emergency resource emergency_manager resource_manager file_manager analytics console app
ORDER_S := types emergency resource emergency_manager resource_manager file_manager analytics console app main

.PHONY: all test single clean

all: resq

resq: $(CORE) src/app.cpp src/main.cpp build/resq_c.o $(wildcard include/*.h)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE) src/app.cpp src/main.cpp build/resq_c.o -o $@

resq_tests: $(CORE) tests/test_resq.cpp build/resq_c.o $(wildcard include/*.h)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE) tests/test_resq.cpp build/resq_c.o -o $@

test: resq_tests
	./resq_tests

build/resq_c.o: src/resq_c.c include/resq_c.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

single:
	{ sed -e '/#include "resq_c.h"/{r include/resq_c.h' -e 'd;}' src/resq_c.c; \
	  for h in $(filter-out resq_c,$(ORDER_H)); do grep -v '^#include "' include/$$h.h; done; \
	  for s in $(ORDER_S); do grep -v '^#include "' src/$$s.cpp; done; } > resq_single.cpp

clean:
	rm -rf build resq resq.exe resq_tests resq_tests.exe
