# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

# Programs
PRODUCER = producer
CONSUMER = consumer

# Source files
PRODUCER_SRC = producer.cpp
CONSUMER_SRC = consumer.cpp

# Build both programs
all: $(PRODUCER) $(CONSUMER)

# Build producer
$(PRODUCER): $(PRODUCER_SRC)
	$(CXX) $(CXXFLAGS) -o $(PRODUCER) $(PRODUCER_SRC) -pthread -lrt

# Build consumer
$(CONSUMER): $(CONSUMER_SRC)
	$(CXX) $(CXXFLAGS) -o $(CONSUMER) $(CONSUMER_SRC) -pthread -lrt

# Remove compiled binaries
clean:
	rm -f $(PRODUCER) $(CONSUMER)

.PHONY: all clean

