CC=g++
CFLAGS=-std=c++17 -g -pthread
TARGET=main

all: $(TARGET)

$(TARGET): test.cpp threadpool.cpp
		$(CC) $(CFLAGS) -o $(TARGET) test.cpp threadpool.cpp

clean:
		rm -f $(TARGET)

