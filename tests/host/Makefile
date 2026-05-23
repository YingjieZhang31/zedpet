CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O0 -g -I../../src

BIN = test_claude_client_parsing
SRCS = test_claude_client_parsing.cpp ../../src/claude_client_parsing.cpp

all: $(BIN)
	./$(BIN)

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(BIN)
