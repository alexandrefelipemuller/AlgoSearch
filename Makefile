CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2 -Isrc -pthread
LDFLAGS ?= -pthread

SRC_DIR := src
TEST_DIR := tests
BUILD_DIR := build

LIB_SOURCES := $(shell find $(SRC_DIR) -name '*.cpp' ! -name 'main.cpp')
LIB_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/lib/%.o,$(LIB_SOURCES))

TEST_SOURCES := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SOURCES))

MAIN_OBJECT := $(BUILD_DIR)/lib/main.o

.PHONY: all test clean

all: algosearch

algosearch: $(LIB_OBJECTS) $(MAIN_OBJECT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

algosearch_tests: $(LIB_OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -I$(TEST_DIR) -o $@ $^ $(LDFLAGS)

test: algosearch_tests
	./algosearch_tests

$(BUILD_DIR)/lib/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(TEST_DIR) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) algosearch algosearch_tests
