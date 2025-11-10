CC = gcc

TARGET = demo
SRC = my_thread.c main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC)  $^ -o $@

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
