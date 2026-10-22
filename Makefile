CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS  ?=

BUILD := build

# Core library shared by the server, the client and the unit tests.
LIB_SRC  := net.cpp protocol.cpp hashtable.cpp
LIB_OBJ  := $(LIB_SRC:%.cpp=$(BUILD)/%.o)
TEST_SRC := $(wildcard tests/unit/*.cpp)
TEST_OBJ := $(TEST_SRC:tests/unit/%.cpp=$(BUILD)/tests/%.o)

.PHONY: all test unit clean

all: $(BUILD)/nanoredis-server $(BUILD)/nanoredis-cli

$(BUILD)/nanoredis-server: $(BUILD)/server.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/nanoredis-cli: $(BUILD)/client.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/unit-tests: $(TEST_OBJ) $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/tests/%.o: tests/unit/%.cpp | $(BUILD)
	@mkdir -p $(BUILD)/tests
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

unit: $(BUILD)/unit-tests
	./$(BUILD)/unit-tests

test: unit

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d $(BUILD)/tests/*.d)
