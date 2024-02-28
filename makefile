CC = gcc
CFLAGS = `pkg-config --cflags --libs gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`

SRC = sudoku.c
TARGET = sudoku

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS) -g -rdynamic




