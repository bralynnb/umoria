CXX ?= g++
CXXFLAGS ?= -O2 -g
CPPFLAGS += -DMORIA_COOP -Isrc
SOURCES := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
OBJECTS := $(patsubst src/%.cpp,build-web/obj/%.o,$(SOURCES))
HEADERS := $(wildcard src/*.h)
.PHONY: all
all: build-web/umoria-coop
build-web/obj/%.o: src/%.cpp $(HEADERS)
	@mkdir -p build-web/obj
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++14 -c $< -o $@
build-web/umoria-coop: $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@
build-web/engine-tests: tests/engine.cpp $(filter-out build-web/obj/coop.o,$(OBJECTS)) $(HEADERS) src/coop.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++14 tests/engine.cpp $(filter-out build-web/obj/coop.o,$(OBJECTS)) -o $@
