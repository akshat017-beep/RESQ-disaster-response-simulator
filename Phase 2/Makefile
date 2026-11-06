CC = gcc
CXX = g++
CFLAGS = -std=c11 -Wall -Wextra -pedantic -Iinclude
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -Iinclude

OBJECTS = victim_triage.o data_structures.o graph_algorithms.o rescue_system.o main.o

all: resq

victim_triage.o: src/c/victim_triage.c include/resq_c.h
	$(CC) $(CFLAGS) -c $< -o $@

data_structures.o: src/c/data_structures.c include/resq_c.h
	$(CC) $(CFLAGS) -c $< -o $@

graph_algorithms.o: src/c/graph_algorithms.c include/resq_c.h
	$(CC) $(CFLAGS) -c $< -o $@

rescue_system.o: src/cpp/rescue_system.cpp include/rescue_system.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: src/cpp/main.cpp include/rescue_system.h include/resq_c.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

resq: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o resq

clean:
	rm -f $(OBJECTS) resq resq.exe

.PHONY: all clean
