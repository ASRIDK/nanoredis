CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS  ?=

BUILD := build

.PHONY: all clean

all: $(BUILD)/nanoredis-server $(BUILD)/nanoredis-cli

$(BUILD)/nanoredis-server: $(BUILD)/server.o $(BUILD)/net.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/nanoredis-cli: $(BUILD)/client.o $(BUILD)/net.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
