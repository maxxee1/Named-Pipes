CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

BIN := server guardian client

all: $(BIN)

server: server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

guardian: GuardianReport.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

client: client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(BIN)
