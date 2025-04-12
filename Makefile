CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
LDFLAGS = -pg

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

all: test

test: tests.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o tests tests.cpp engine.cpp
	./tests

gprofTest: tests.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o tests tests.cpp engine.cpp
		./tests
	gprof tests gmon.out > report.txt

submit: engine.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c engine.cpp -o engine.o
	$(CXX) $(CXXFLAGS) -shared -o engine.so engine.o
	lll-bench $(MAKEFILE_DIR)engine.so -d 1

clean:
	rm -f tests engine.o engine.so gmon.out report.txt
