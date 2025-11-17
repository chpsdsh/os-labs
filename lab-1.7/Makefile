CC      = gcc
LDFLAGS = 

SRC     = uthread.c main.c
OBJ     = $(SRC:.c=.o)
TARGET  = demo

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

%.o: %.c uthread.h
	$(CC)  -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)
