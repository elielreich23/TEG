CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99

OBJS = graph.o main.o
TARGET = program

.PHONY: all clean test

all: $(TARGET) test_runner

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

graph.o: graph.c graph.h
main.o: main.c graph.h

test_runner: graph.o tests/test_runner.o
	$(CC) $(CFLAGS) -o $@ graph.o tests/test_runner.o

tests/test_runner.o: tests/test_runner.c graph.h

test: test_runner
	./test_runner 2>/dev/null || ./test_runner.exe

clean:
	rm -f $(OBJS) tests/test_runner.o $(TARGET) test_runner $(TARGET).exe test_runner.exe
