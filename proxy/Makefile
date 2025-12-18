CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := -pthread

TARGET  := proxy

SRCS := \
    main.c \
    proxy.c \
    cache.c \
    threadpool.c

OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
