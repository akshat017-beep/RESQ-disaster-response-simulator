# RESQ - Intelligent Disaster Rescue & Emergency Response Simulator
# Build:  make        Run:  make run        Clean: make clean

CC       := gcc
CXX      := g++
CFLAGS   := -std=c11   -Wall -Wextra -Iinclude -O2
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -O2

CSRC   := $(wildcard src/c/*.c)
CPPSRC := $(wildcard src/cpp/*.cpp)
COBJ   := $(CSRC:.c=.o)
CPPOBJ := $(CPPSRC:.cpp=.o)
BIN    := resq

all: $(BIN)

$(BIN): $(COBJ) $(CPPOBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(COBJ) $(CPPOBJ) $(BIN)

.PHONY: all run clean
