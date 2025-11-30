CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -pthread
TARGET  := spin_demo

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.c spin.c
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
