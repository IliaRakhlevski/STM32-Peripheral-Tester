CC = gcc

CFLAGS = -Wall -Wextra -Werror -pthread

LDLIBS = -lsqlite3

SRCS = $(wildcard *.c)

TARGET = stm32_server

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET)
