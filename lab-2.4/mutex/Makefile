CC      := gcc
TARGET  := mutex_demo

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.c mutex.c
	$(CC)  $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
