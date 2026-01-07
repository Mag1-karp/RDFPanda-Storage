CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

TARGET = dag_generator
SOURCE = dag_generator.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

.PHONY: clean

# Usage examples:
# make
# ./dag_generator 100 200 test_dag.ttl
# ./dag_generator 1000 2000 large_dag.ttl