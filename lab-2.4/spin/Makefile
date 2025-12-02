CC      := gcc
TARGET  := spin_demo

.PHONY: all clean run

all: $(TARGET)

$(TARGET): main.c spin.c
	$(CC)  $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
